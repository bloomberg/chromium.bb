// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_USB_USB_PRIVATE_API_H_

#include "extensions/browser/api/usb/usb_api.h"
#include "extensions/common/api/usb_private.h"

namespace extensions {

class UsbPrivateGetDevicesFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usbPrivate.getDevices", USBPRIVATE_GETDEVICES)

  UsbPrivateGetDevicesFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~UsbPrivateGetDevicesFunction();

 private:
  scoped_ptr<extensions::core_api::usb_private::GetDevices::Params> parameters_;
};

class UsbPrivateGetDeviceInfoFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usbPrivate.getDeviceInfo",
                             USBPRIVATE_GETDEVICEINFO)

  UsbPrivateGetDeviceInfoFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~UsbPrivateGetDeviceInfoFunction();

 private:
  scoped_ptr<extensions::core_api::usb_private::GetDeviceInfo::Params>
      parameters_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_API_H_
