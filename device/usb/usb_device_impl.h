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
  virtual void RequestUsbAccess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) OVERRIDE;
#endif  // OS_CHROMEOS
  virtual scoped_refptr<UsbDeviceHandle> Open() OVERRIDE;
  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) OVERRIDE;
  virtual const UsbConfigDescriptor& GetConfiguration() OVERRIDE;
  virtual bool GetManufacturer(base::string16* manufacturer) OVERRIDE;
  virtual bool GetProduct(base::string16* product) OVERRIDE;
  virtual bool GetSerialNumber(base::string16* serial_number) OVERRIDE;

 protected:
  friend class UsbServiceImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
                scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                PlatformUsbDevice platform_device,
                uint16 vendor_id,
                uint16 product_id,
                uint32 unique_id);

  virtual ~UsbDeviceImpl();

  // Called only be UsbService.
  void OnDisconnect();

 private:
  base::ThreadChecker thread_checker_;
  PlatformUsbDevice platform_device_;

#if defined(USE_UDEV)
  // On Linux these properties are read from sysfs when the device is enumerated
  // to avoid hitting the permission broker on Chrome OS for a real string
  // descriptor request.
  std::string manufacturer_;
  std::string product_;
  std::string serial_number_;
#endif

  // The active configuration descriptor is not read immediately but cached for
  // later use.
  bool current_configuration_cached_;
  UsbConfigDescriptor current_configuration_;

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
