// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_USB_USB_DEVICE_IMPL_H_
#define DEVICE_USB_USB_DEVICE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device.h"

struct libusb_device;
struct libusb_config_descriptor;

namespace base {
class SingleThreadTaskRunner;
}

namespace device {

class UsbDeviceHandleImpl;
class UsbContext;

typedef libusb_device* PlatformUsbDevice;
typedef libusb_config_descriptor* PlatformUsbConfigDescriptor;

class UsbDeviceImpl : public UsbDevice {
 public:
// UsbDevice implementation:
#if defined(OS_CHROMEOS)
  void RequestUsbAccess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) override;
#endif  // OS_CHROMEOS
  scoped_refptr<UsbDeviceHandle> Open() override;
  bool Close(scoped_refptr<UsbDeviceHandle> handle) override;
  const UsbConfigDescriptor* GetConfiguration() override;
  bool GetManufacturer(base::string16* manufacturer) override;
  bool GetProduct(base::string16* product) override;
  bool GetSerialNumber(base::string16* serial_number) override;

 protected:
  friend class UsbServiceImpl;
  friend class UsbDeviceHandleImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
                scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                PlatformUsbDevice platform_device,
                uint16 vendor_id,
                uint16 product_id,
                uint32 unique_id);

  ~UsbDeviceImpl() override;

  // Called only by UsbServiceImpl.
  void OnDisconnect();

  // Called by UsbDeviceHandleImpl.
  void RefreshConfiguration();

 private:
  base::ThreadChecker thread_checker_;
  PlatformUsbDevice platform_device_;

  // On Linux these properties are read from sysfs when the device is enumerated
  // to avoid hitting the permission broker on Chrome OS for a real string
  // descriptor request.
  base::string16 manufacturer_;
  base::string16 product_;
  base::string16 serial_number_;
#if !defined(USE_UDEV)
  // On other platforms the device must be opened in order to cache them. This
  // should be delayed until the strings are needed to avoid poor interactions
  // with other applications.
  void CacheStrings();
  bool strings_cached_;
#endif
#if defined(OS_CHROMEOS)
  // On Chrome OS save the devnode string for requesting path access from
  // permission broker.
  std::string devnode_;
#endif

  // The current device configuration descriptor. May be null if the device is
  // in an unconfigured state.
  scoped_ptr<UsbConfigDescriptor> configuration_;

  // Retain the context so that it will not be released before UsbDevice.
  scoped_refptr<UsbContext> context_;

  // Opened handles.
  typedef std::vector<scoped_refptr<UsbDeviceHandleImpl> > HandlesVector;
  HandlesVector handles_;

  // Reference to the UI thread for permission-broker calls.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceImpl);
};

}  // namespace device

#endif  // DEVICE_USB_USB_DEVICE_IMPL_H_
