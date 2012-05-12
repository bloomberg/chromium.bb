// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/common/extensions/api/experimental_usb.h"

namespace extensions {

class APIResourceEventNotifier;

class UsbFindDeviceFunction : public AsyncAPIFunction {
 public:
  UsbFindDeviceFunction();
  virtual ~UsbFindDeviceFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::FindDevice::Params> parameters_;
  APIResourceEventNotifier* event_notifier_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.findDevice");
};

class UsbCloseDeviceFunction : public AsyncAPIFunction {
 public:
  UsbCloseDeviceFunction();
  virtual ~UsbCloseDeviceFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::CloseDevice::Params>
      parameters_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.closeDevice");
};

class UsbControlTransferFunction : public AsyncAPIFunction {
 public:
  UsbControlTransferFunction();
  virtual ~UsbControlTransferFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::ControlTransfer::Params>
      parameters_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.controlTransfer");
};

class UsbBulkTransferFunction : public AsyncAPIFunction {
 public:
  UsbBulkTransferFunction();
  virtual ~UsbBulkTransferFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::BulkTransfer::Params>
      parameters_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.bulkTransfer");
};

class UsbInterruptTransferFunction : public AsyncAPIFunction {
 public:
  UsbInterruptTransferFunction();
  virtual ~UsbInterruptTransferFunction();

 protected:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::InterruptTransfer::Params>
      parameters_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.interruptTransfer");
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
