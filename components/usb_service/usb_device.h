// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USB_SERVICE_USB_DEVICE_H_
#define COMPONENTS_USB_SERVICE_USB_DEVICE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/usb_service/usb_service_export.h"

namespace usb_service {

class UsbDeviceHandle;
class UsbConfigDescriptor;

// A UsbDevice object represents a detected USB device, providing basic
// information about it. For further manipulation of the device, a
// UsbDeviceHandle must be created from Open() method.
class USB_SERVICE_EXPORT UsbDevice
    : public base::RefCountedThreadSafe<UsbDevice> {
 public:
  // Accessors to basic information.
  uint16 vendor_id() const { return vendor_id_; }
  uint16 product_id() const { return product_id_; }
  uint32 unique_id() const { return unique_id_; }

#if defined(OS_CHROMEOS)
  // On ChromeOS, if an interface of a claimed device is not claimed, the
  // permission broker can change the owner of the device so that the unclaimed
  // interfaces can be used. If this argument is missing, permission broker will
  // not be used and this method fails if the device is claimed.
  virtual void RequestUsbAcess(
      int interface_id,
      const base::Callback<void(bool success)>& callback) = 0;
#endif  // OS_CHROMEOS

  // Creates a UsbDeviceHandle for further manipulation.
  // Blocking method. Must be called on FILE thread.
  virtual scoped_refptr<UsbDeviceHandle> Open() = 0;

  // Explicitly closes a device handle. This method will be automatically called
  // by the destructor of a UsbDeviceHandle as well.
  // Closing a closed handle is a safe
  // Blocking method. Must be called on FILE thread.
  virtual bool Close(scoped_refptr<UsbDeviceHandle> handle) = 0;

  // Lists the interfaces provided by the device and fills the given
  // UsbConfigDescriptor.
  // Blocking method. Must be called on FILE thread.
  virtual scoped_refptr<UsbConfigDescriptor> ListInterfaces() = 0;

 protected:
  UsbDevice(uint16 vendor_id, uint16 product_id, uint32 unique_id)
      : vendor_id_(vendor_id), product_id_(product_id), unique_id_(unique_id) {}

  virtual ~UsbDevice() {}

 private:
  friend class base::RefCountedThreadSafe<UsbDevice>;

  const uint16 vendor_id_;
  const uint16 product_id_;
  const uint32 unique_id_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace usb_service

#endif  // COMPONENTS_USB_SERVICE_USB_DEVICE_H_
