// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/usb/usb_service.h"

#include <map>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "device/usb/usb_context.h"
#include "device/usb/usb_device_impl.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

typedef struct libusb_device* PlatformUsbDevice;
typedef struct libusb_context* PlatformUsbContext;

class UsbServiceImpl : public UsbService {
 public:
  static UsbService* Create(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

 private:
  explicit UsbServiceImpl(
      PlatformUsbContext context,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~UsbServiceImpl() override;

  // device::UsbService implementation
  scoped_refptr<UsbDevice> GetDeviceById(uint32 unique_id) override;
  void GetDevices(std::vector<scoped_refptr<UsbDevice>>* devices) override;

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

#if defined(OS_WIN)
  class UIThreadHelper;
  UIThreadHelper* ui_thread_helper_;
#endif  // OS_WIN

  // TODO(reillyg): Figure out a better solution for device IDs.
  uint32 next_unique_id_;

  // When available the device list will be updated when new devices are
  // connected instead of only when a full enumeration is requested.
  // TODO(reillyg): Support this on all platforms. crbug.com/411715
  bool hotplug_enabled_;
  libusb_hotplug_callback_handle hotplug_handle_;

  // The map from unique IDs to UsbDevices.
  typedef std::map<uint32, scoped_refptr<UsbDeviceImpl>> DeviceMap;
  DeviceMap devices_;

  // The map from PlatformUsbDevices to UsbDevices.
  typedef std::map<PlatformUsbDevice, scoped_refptr<UsbDeviceImpl>>
      PlatformDeviceMap;
  PlatformDeviceMap platform_devices_;

  base::WeakPtrFactory<UsbServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsbServiceImpl);
};

}  // namespace device
