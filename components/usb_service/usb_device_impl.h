// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_DEVICE_IMPL_H_
#define COMPONENTS_USB_SERVICE_USB_DEVICE_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "components/usb_service/usb_device.h"

struct libusb_device;
struct libusb_config_descriptor;

namespace usb_service {

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
  virtual scoped_refptr<UsbConfigDescriptor> ListInterfaces() OVERRIDE;

 protected:
  friend class UsbServiceImpl;

  // Called by UsbServiceImpl only;
  UsbDeviceImpl(scoped_refptr<UsbContext> context,
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

  // Retain the context so that it will not be released before UsbDevice.
  scoped_refptr<UsbContext> context_;

  // Opened handles.
  typedef std::vector<scoped_refptr<UsbDeviceHandleImpl> > HandlesVector;
  HandlesVector handles_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceImpl);
};

}  // namespace usb_service

#endif  // COMPONENTS_USB_SERVICE_USB_DEVICE_IMPL_H_
