#include <poplar/DeviceManager.hpp>

#include <dmlc/thread_local.h>

#include <tvm/runtime/device_api.h>
#include <tvm/runtime/registry.h>

namespace tvm {
namespace runtime {
namespace contrib {

class IPUThreadEntry;

class IPUDeviceAPI final : public DeviceAPI {
public:
  void SetDevice(TVMContext ctx) final;
  void GetAttr(TVMContext ctx, DeviceAttrKind kind, TVMRetValue* rv) final;
  void* AllocDataSpace(TVMContext ctx, size_t nbytes, size_t alignment,
		       DLDataType type_hint) final;
  void FreeDataSpace(TVMContext ctx, void* ptr) final;
  void CopyDataFromTo(const void* from, size_t from_offset,
		      void* to, size_t to_offset, size_t size,
		      TVMContext ctx_from, TVMContext ctx_to,
		      DLDataType type_hint, TVMStreamHandle stream) final;
  void StreamSync(TVMContext ctx, TVMStreamHandle stream) final {}

  IPUThreadEntry* GetThreadEntry();

  IPUDeviceAPI() : m_(poplar::DeviceManager::createDeviceManager()) {}

  static const std::shared_ptr<IPUDeviceAPI>& Global() {
    static std::shared_ptr<IPUDeviceAPI> inst = std::make_shared<IPUDeviceAPI>();
    return inst;
  }
protected:
  poplar::DeviceManager m_;
};

class IPUThreadEntry {
  public:
  // The current context (device)
  TVMContext context;

  IPUThreadEntry(DLDeviceType device_type, std::shared_ptr<DeviceAPI> device) {
    context.device_id = 0;
    context.device_type = device_type;
  }

  IPUThreadEntry() : IPUThreadEntry(static_cast<DLDeviceType>(kDLIPU), IPUDeviceAPI::Global()) {}

  static IPUThreadEntry* ThreadLocal() {
    return dmlc::ThreadLocalStore<IPUThreadEntry>::Get();
  }
};

IPUThreadEntry* IPUDeviceAPI::GetThreadEntry() { return IPUThreadEntry::ThreadLocal(); }

void IPUDeviceAPI::SetDevice(TVMContext ctx) {
  GetThreadEntry()->context.device_id = ctx.device_id;
}

void IPUDeviceAPI::GetAttr(TVMContext ctx, DeviceAttrKind kind, TVMRetValue* rv) {
  size_t index = static_cast<size_t>(ctx.device_id);
  if (kind == kExist) {
    *rv = static_cast<int>(index < m_.getNumDevices());
    return;
  }
  CHECK_LT(index, m_.getNumDevices()) << "Invalid device id " << index;

  // None of the properties for IPU seem relevant for these so we just
  // fake them for now.

  switch(kind) {
    case kDeviceName: {
      *rv = std::string("IPU");
      break;
    }
    case kMaxClockRate: {
      *rv = 1300;
      break;
    }
    case kMultiProcessorCount: {
      *rv = 1216;
      break;
    }
    default:
      return;
  }
}

void* IPUDeviceAPI::AllocDataSpace(TVMContext ctx, size_t nbytes, size_t alignment,
				   DLDataType type_hint) {
  // We allocate the buffers on the CPU since it's not really possible
  // to allocate on the device. We ignore alignment since this is not
  // the location that will be referenced for execution.
  void* ptr;
  ptr = malloc(nbytes);
  if (ptr == nullptr) throw std::bad_alloc();
  return ptr;
}

void IPUDeviceAPI::FreeDataSpace(TVMContext ctx, void* ptr)  {
  free(ptr);
}

void IPUDeviceAPI::CopyDataFromTo(const void* from, size_t from_offset,
				  void* to, size_t to_offset, size_t size,
				  TVMContext ctx_from, TVMContext ctx_to,
				  DLDataType type_hint, TVMStreamHandle stream) {
  memcpy(static_cast<char*>(to) + to_offset, static_cast<const char*>(from) + from_offset, size);
}


TVM_REGISTER_GLOBAL("device_api.ipu").set_body([](TVMArgs args, TVMRetValue* rv) {
    DeviceAPI* ptr = IPUDeviceAPI::Global().get();
    *rv = static_cast<void*>(ptr);
});

}
}
}
