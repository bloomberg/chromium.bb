// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_

#include <string>

#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/common/extensions/api/experimental_usb.h"

class UsbDevice;

namespace extensions {

class ApiResourceEventNotifier;
class UsbDeviceResource;

class UsbAsyncApiFunction : public AsyncApiFunction {
 public:
  UsbAsyncApiFunction();

 protected:
  virtual ~UsbAsyncApiFunction();

  virtual bool PrePrepare() OVERRIDE;

  UsbDeviceResource* GetUsbDeviceResource(int api_resource_id);
  void RemoveUsbDeviceResource(int api_resource_id);

  ApiResourceManager<UsbDeviceResource>* manager_;
};

class UsbFindDeviceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.findDevice");

  UsbFindDeviceFunction();

  static void SetDeviceForTest(UsbDevice* device);

 protected:
  virtual ~UsbFindDeviceFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::FindDevice::Params> parameters_;
  ApiResourceEventNotifier* event_notifier_;
};

class UsbCloseDeviceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.closeDevice");

  UsbCloseDeviceFunction();

 protected:
  virtual ~UsbCloseDeviceFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::CloseDevice::Params>
      parameters_;
};

class UsbControlTransferFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.controlTransfer");

  UsbControlTransferFunction();

 protected:
  virtual ~UsbControlTransferFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::ControlTransfer::Params>
      parameters_;
};

class UsbBulkTransferFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.bulkTransfer");

  UsbBulkTransferFunction();

 protected:
  virtual ~UsbBulkTransferFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::BulkTransfer::Params>
      parameters_;
};

class UsbInterruptTransferFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.interruptTransfer");

  UsbInterruptTransferFunction();

 protected:
  virtual ~UsbInterruptTransferFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::InterruptTransfer::Params>
      parameters_;
};

class UsbIsochronousTransferFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.usb.isochronousTransfer");

  UsbIsochronousTransferFunction();

 protected:
  virtual ~UsbIsochronousTransferFunction();
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  scoped_ptr<extensions::api::experimental_usb::IsochronousTransfer::Params>
      parameters_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
