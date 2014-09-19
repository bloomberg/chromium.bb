// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include <map>
#include <set>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
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
  virtual ~UsbServiceImpl();

 private:
  // device::UsbService implementation
  virtual scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) OVERRIDE;
  virtual void GetDevices(
      std::vector<scoped_refptr<UsbDevice> >* devices) OVERRIDE;

  // base::MessageLoop::DestructionObserver implementation.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Enumerate USB devices from OS and update devices_ map.
  void RefreshDevices();

  scoped_refptr<UsbContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // TODO(reillyg): Figure out a better solution.
  uint32 next_unique_id_;

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
  RefreshDevices();

  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    devices->push_back(it->second);
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
      next_unique_id_(0) {
  base::MessageLoop::current()->AddDestructionObserver(this);
}

UsbServiceImpl::~UsbServiceImpl() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    it->second->OnDisconnect();
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
      libusb_device_descriptor descriptor;
      const int rv =
          libusb_get_device_descriptor(platform_devices[i], &descriptor);
      // This test is needed. A valid vendor/produce pair is required.
      if (rv != LIBUSB_SUCCESS) {
        VLOG(1) << "Failed to get device descriptor: "
                << ConvertPlatformUsbErrorToString(rv);
        continue;
      }

      uint32 unique_id;
      do {
        unique_id = ++next_unique_id_;
      } while (devices_.find(unique_id) != devices_.end());

      scoped_refptr<UsbDeviceImpl> new_device(
          new UsbDeviceImpl(context_,
                            ui_task_runner_,
                            platform_devices[i],
                            descriptor.idVendor,
                            descriptor.idProduct,
                            unique_id));
      platform_devices_[platform_devices[i]] = new_device;
      devices_[unique_id] = new_device;
      connected_devices.insert(new_device.get());
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
