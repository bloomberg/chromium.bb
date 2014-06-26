// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/usb_service/usb_service.h"

#include <map>
#include <set>

#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "components/usb_service/usb_context.h"
#include "components/usb_service/usb_device_impl.h"
#include "components/usb_service/usb_error.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace usb_service {

namespace {

base::LazyInstance<scoped_ptr<UsbService> >::Leaky g_usb_service_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbServiceImpl
    : public UsbService,
      private base::MessageLoop::DestructionObserver {
 public:
  explicit UsbServiceImpl(PlatformUsbContext context);
  virtual ~UsbServiceImpl();

 private:
  // usb_service::UsbService implementation
  virtual scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) OVERRIDE;
  virtual void GetDevices(
      std::vector<scoped_refptr<UsbDevice> >* devices) OVERRIDE;

  // base::MessageLoop::DestructionObserver implementation.
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  // Enumerate USB devices from OS and Update devices_ map.
  void RefreshDevices();

  scoped_refptr<UsbContext> context_;

  // TODO(ikarienator): Figure out a better solution.
  uint32 next_unique_id_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceImpl> > DeviceMap;
  DeviceMap devices_;

  DISALLOW_COPY_AND_ASSIGN(UsbServiceImpl);
};

scoped_refptr<UsbDevice> UsbServiceImpl::GetDeviceById(uint32 unique_id) {
  DCHECK(CalledOnValidThread());
  RefreshDevices();
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (it->second->unique_id() == unique_id)
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

UsbServiceImpl::UsbServiceImpl(PlatformUsbContext context)
    : context_(new UsbContext(context)), next_unique_id_(0) {
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
            << ConvertErrorToString(device_count);
  }

  std::set<UsbDevice*> connected_devices;
  std::vector<PlatformUsbDevice> disconnected_devices;

  // Populates new devices.
  for (ssize_t i = 0; i < device_count; ++i) {
    if (!ContainsKey(devices_, platform_devices[i])) {
      libusb_device_descriptor descriptor;
      const int rv =
          libusb_get_device_descriptor(platform_devices[i], &descriptor);
      // This test is needed. A valid vendor/produce pair is required.
      if (rv != LIBUSB_SUCCESS) {
        VLOG(1) << "Failed to get device descriptor: "
                << ConvertErrorToString(rv);
        continue;
      }
      UsbDeviceImpl* new_device = new UsbDeviceImpl(context_,
                                                    platform_devices[i],
                                                    descriptor.idVendor,
                                                    descriptor.idProduct,
                                                    ++next_unique_id_);
      devices_[platform_devices[i]] = new_device;
      connected_devices.insert(new_device);
    } else {
      connected_devices.insert(devices_[platform_devices[i]].get());
    }
  }

  // Find disconnected devices.
  for (DeviceMap::iterator it = devices_.begin(); it != devices_.end(); ++it) {
    if (!ContainsKey(connected_devices, it->second)) {
      disconnected_devices.push_back(it->first);
    }
  }

  // Remove disconnected devices from devices_.
  for (size_t i = 0; i < disconnected_devices.size(); ++i) {
    // UsbDevice will be destroyed after this. The corresponding
    // PlatformUsbDevice will be unref'ed during this process.
    devices_.erase(disconnected_devices[i]);
  }

  libusb_free_device_list(platform_devices, true);
}

// static
UsbService* UsbService::GetInstance() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  UsbService* instance = g_usb_service_instance.Get().get();
  if (!instance) {
    PlatformUsbContext context = NULL;

    const int rv = libusb_init(&context);
    if (rv != LIBUSB_SUCCESS) {
      VLOG(1) << "Failed to initialize libusb: " << ConvertErrorToString(rv);
      return NULL;
    }
    if (!context)
      return NULL;

    instance = new UsbServiceImpl(context);
    g_usb_service_instance.Get().reset(instance);
  }
  return instance;
}

// static
void UsbService::SetInstanceForTest(UsbService* instance) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  g_usb_service_instance.Get().reset(instance);
}

}  // namespace usb_service
