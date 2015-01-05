// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_device_impl.h"
#include "device/usb/usb_error.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

namespace {

base::LazyInstance<scoped_ptr<UsbService> >::Leaky g_usb_service_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbServiceImpl : public UsbService,
                       private base::MessageLoop::DestructionObserver {
 public:
  explicit UsbServiceImpl(
      PlatformUsbContext context,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~UsbServiceImpl() override;

 private:
  // device::UsbService implementation
  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override;
  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override;

  // base::MessageLoop::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  // Enumerate USB devices from OS and update devices_ map.
  void RefreshDevices();

  // Adds a new UsbDevice to the devices_ map based on the given libusb device.
  scoped_refptr<UsbDeviceImpl> AddDevice(PlatformUsbDevice platform_device);

  // Handle hotplug events from libusb.
  static int LIBUSB_CALL HotplugCallback(libusb_context* context,
                                         PlatformUsbDevice device,
                                         libusb_hotplug_event event,
                                         void* user_data);
  // These functions release a reference to the provided platform device.
  void OnDeviceAdded(PlatformUsbDevice platform_device);
  void OnDeviceRemoved(PlatformUsbDevice platform_device);

  scoped_refptr<UsbContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // TODO(reillyg): Figure out a better solution for device IDs.
  uint32 next_unique_id_;

  // When available the device list will be updated when new devices are
  // connected instead of only when a full enumeration is requested.
  // TODO(reillyg): Support this on all platforms. crbug.com/411715
  bool hotplug_enabled_;
  libusb_hotplug_callback_handle hotplug_handle_;

  // The map from unique IDs to UsbDevices.
  typedef std::map<uint32, scoped_refptr<UsbDeviceImpl> > DeviceMap;
  DeviceMap devices_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceImpl> >
      PlatformDeviceMap;
  PlatformDeviceMap platform_devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbServiceImpl);
};

scoped_refptr<UsbDevice> UsbServiceImpl::GetDeviceById(uint32 unique_id) {
  DCHECK(CalledOnValidThread());
  RefreshDevices();
  DeviceMap::iterator it = devices_.find(unique_id);
  if (it != devices_.end()) {
    return it->second;
  }
  return NULL;
}

void UsbServiceImpl::GetDevices(
    std::vector<scoped_refptr<UsbDevice> >* devices) {
  DCHECK(CalledOnValidThread());
  STLClearObject(devices);

  if (!hotplug_enabled_) {
    RefreshDevices();
  }

  for (const auto& map_entry : devices_) {
    devices->push_back(map_entry.second);
  }
}

void UsbServiceImpl::WillDestroyCurrentMessageLoop() {
  DCHECK(CalledOnValidThread());
  g_usb_service_instance.Get().reset(NULL);
}

UsbServiceImpl::UsbServiceImpl(
    PlatformUsbContext context,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
    : context_(new UsbContext(context)),
      ui_task_runner_(ui_task_runner),
      next_unique_id_(0),
      hotplug_enabled_(false) {
  base::MessageLoop::current()->AddDestructionObserver(this);
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  int rv = libusb_hotplug_register_callback(
      context_->context(),
      static_cast<libusb_hotplug_event>(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                        LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT),
      LIBUSB_HOTPLUG_ENUMERATE, LIBUSB_HOTPLUG_MATCH_ANY,
      LIBUSB_HOTPLUG_MATCH_ANY, LIBUSB_HOTPLUG_MATCH_ANY,
      &UsbServiceImpl::HotplugCallback, this, &hotplug_handle_);
  if (rv == LIBUSB_SUCCESS) {
    hotplug_enabled_ = true;
  }
}

UsbServiceImpl::~UsbServiceImpl() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  if (hotplug_enabled_) {
    libusb_hotplug_deregister_callback(context_->context(), hotplug_handle_);
  }
  for (const auto& map_entry : devices_) {
    map_entry.second->OnDisconnect();
  }
}

void UsbServiceImpl::RefreshDevices() {
  DCHECK(CalledOnValidThread());

  libusb_device** platform_devices = NULL;
  const ssize_t device_count =
      libusb_get_device_list(context_->context(), &platform_devices);
  if (device_count < 0) {
    VLOG(1) << "Failed to get device list: "
            << ConvertPlatformUsbErrorToString(device_count);
  }

  std::set<UsbDevice*> connected_devices;
  std::vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    if (!ContainsKey(platform_devices_, platform_devices[i])) {
      scoped_refptr<UsbDeviceImpl> new_device = AddDevice(platform_devices[i]);
      if (new_device) {
        connected_devices.insert(new_device.get());
      }
    } else {
      connected_devices.insert(platform_devices_[platform_devices[i]].get());
    }
  }

  // Find disconnected devices.
  for (PlatformDeviceMap::iterator it = platform_devices_.begin();
       it != platform_devices_.end();
       ++it) {
    if (!ContainsKey(connected_devices, it->second.get())) {
      disconnected_devices.push_back(it->first);
      devices_.erase(it->second->unique_id());
      it->second->OnDisconnect();
    }
  }

  // Remove disconnected devices from platform_devices_.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    // UsbDevice will be destroyed after this. The corresponding
    // PlatformUsbDevice will be unref'ed during this process.
    platform_devices_.erase(disconnected_devices[i]);
  }

  libusb_free_device_list(platform_devices, true);
}

scoped_refptr<UsbDeviceImpl> UsbServiceImpl::AddDevice(
    PlatformUsbDevice platform_device) {
  libusb_device_descriptor descriptor;
  int rv = libusb_get_device_descriptor(platform_device, &descriptor);
  if (rv == LIBUSB_SUCCESS) {
    uint32 unique_id;
    do {
      unique_id = ++next_unique_id_;
    } while (devices_.find(unique_id) != devices_.end());

    scoped_refptr<UsbDeviceImpl> new_device(new UsbDeviceImpl(
        context_, ui_task_runner_, platform_device, descriptor.idVendor,
        descriptor.idProduct, unique_id));
    platform_devices_[platform_device] = new_device;
    devices_[unique_id] = new_device;
    return new_device;
  } else {
    VLOG(1) << "Failed to get device descriptor: "
            << ConvertPlatformUsbErrorToString(rv);
    return nullptr;
  }
}

// static
int LIBUSB_CALL UsbServiceImpl::HotplugCallback(libusb_context* context,
                                                PlatformUsbDevice device,
                                                libusb_hotplug_event event,
                                                void* user_data) {
  // It is safe to access the UsbServiceImpl* here because libusb takes a lock
  // around registering, deregistering and calling hotplug callback functions
  // and so guarantees that this function will not be called by the event
  // processing thread after it has been deregistered.
  UsbServiceImpl* self = reinterpret_cast<UsbServiceImpl*>(user_data);
  switch (event) {
    case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
      libusb_ref_device(device);  // Released in OnDeviceAdded.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnDeviceAdded(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnDeviceAdded,
                                  base::Unretained(self), device));
      }
      break;
    case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
      libusb_ref_device(device);  // Released in OnDeviceRemoved.
      if (self->task_runner_->BelongsToCurrentThread()) {
        self->OnDeviceRemoved(device);
      } else {
        self->task_runner_->PostTask(
            FROM_HERE, base::Bind(&UsbServiceImpl::OnDeviceRemoved,
                                  base::Unretained(self), device));
      }
      break;
    default:
      NOTREACHED();
  }

  return 0;
}

void UsbServiceImpl::OnDeviceAdded(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());
  DCHECK(!ContainsKey(platform_devices_, platform_device));

  AddDevice(platform_device);
  libusb_unref_device(platform_device);
}

void UsbServiceImpl::OnDeviceRemoved(PlatformUsbDevice platform_device) {
  DCHECK(CalledOnValidThread());

  PlatformDeviceMap::iterator it = platform_devices_.find(platform_device);
  if (it != platform_devices_.end()) {
    scoped_refptr<UsbDeviceImpl> device = it->second;
    DeviceMap::iterator dev_it = devices_.find(device->unique_id());
    if (dev_it != devices_.end()) {
      devices_.erase(dev_it);
    } else {
      NOTREACHED();
    }
    device->OnDisconnect();
    platform_devices_.erase(it);
  } else {
    NOTREACHED();
  }

  libusb_unref_device(platform_device);
}

// static
UsbService* UsbService::GetInstance(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  UsbService* instance = g_usb_service_instance.Get().get();
  if (!instance) {
    PlatformUsbContext context = NULL;

    const int rv = libusb_init(&context);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to initialize libusb: "
              << ConvertPlatformUsbErrorToString(rv);
      return NULL;
    }
    if (!context)
      return NULL;

    instance = new UsbServiceImpl(context, ui_task_runner);
    g_usb_service_instance.Get().reset(instance);
  }
  return instance;
}

// static
void UsbService::SetInstanceForTest(UsbService* instance) {
  g_usb_service_instance.Get().reset(instance);
}

}  // namespace device
