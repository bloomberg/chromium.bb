// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/usb/usb_device.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/common/extensions/api/usb.h"
#include "net/base/io_buffer.h"

class UsbDevice;
class UsbDeviceHandle;
class UsbService;

namespace extensions {

class UsbDeviceResource;

class UsbAsyncApiFunction : public AsyncApiFunction {
 public:
  UsbAsyncApiFunction();

 protected:
  virtual ~UsbAsyncApiFunction();

  virtual bool PrePrepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;

  scoped_refptr<UsbDevice> GetDeviceOrOrCompleteWithError(
      const extensions::api::usb::Device& input_device);

  scoped_refptr<UsbDeviceHandle> GetDeviceHandleOrCompleteWithError(
      const extensions::api::usb::ConnectionHandle& input_device_handle);

  void RemoveUsbDeviceResource(int api_resource_id);

  void CompleteWithError(const std::string& error);

  ApiResourceManager<UsbDeviceResource>* manager_;
};

class UsbAsyncApiTransferFunction : public UsbAsyncApiFunction {
 protected:
  UsbAsyncApiTransferFunction();
  virtual ~UsbAsyncApiTransferFunction();

  bool ConvertDirectionSafely(const extensions::api::usb::Direction& input,
                              UsbEndpointDirection* output);
  bool ConvertRequestTypeSafely(const extensions::api::usb::RequestType& input,
                              UsbDeviceHandle::TransferRequestType* output);
  bool ConvertRecipientSafely(const extensions::api::usb::Recipient& input,
                              UsbDeviceHandle::TransferRecipient* output);

  void OnCompleted(UsbTransferStatus status,
                   scoped_refptr<net::IOBuffer> data,
                   size_t length);
};

class UsbFindDevicesFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.findDevices", USB_FINDDEVICES)

  UsbFindDevicesFunction();

 protected:
  virtual ~UsbFindDevicesFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  void OpenDevices(scoped_ptr<std::vector<scoped_refptr<UsbDevice> > > devices);

  std::vector<scoped_refptr<UsbDeviceHandle> > device_handles_;
  scoped_ptr<extensions::api::usb::FindDevices::Params> parameters_;
};

class UsbGetDevicesFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.getDevices", USB_GETDEVICES)

  UsbGetDevicesFunction();

  static void SetDeviceForTest(UsbDevice* device);

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~UsbGetDevicesFunction();

 private:
  void EnumerationCompletedFileThread(
      scoped_ptr<std::vector<scoped_refptr<UsbDevice> > > devices);

  scoped_ptr<extensions::api::usb::GetDevices::Params> parameters_;
};

class UsbRequestAccessFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.requestAccess", USB_REQUESTACCESS)

  UsbRequestAccessFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~UsbRequestAccessFunction();

  void OnCompleted(bool success);

 private:
  scoped_ptr<extensions::api::usb::RequestAccess::Params> parameters_;
};

class UsbOpenDeviceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.openDevice", USB_OPENDEVICE)

  UsbOpenDeviceFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 protected:
  virtual ~UsbOpenDeviceFunction();

 private:
  scoped_refptr<UsbDeviceHandle> handle_;
  scoped_ptr<extensions::api::usb::OpenDevice::Params> parameters_;
};

class UsbListInterfacesFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.listInterfaces", USB_LISTINTERFACES)

  UsbListInterfacesFunction();

 protected:
  virtual ~UsbListInterfacesFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  bool ConvertDirectionSafely(const UsbEndpointDirection& input,
                              extensions::api::usb::Direction* output);
  bool ConvertSynchronizationTypeSafely(
      const UsbSynchronizationType& input,
      extensions::api::usb::SynchronizationType* output);
  bool ConvertTransferTypeSafely(const UsbTransferType& input,
                              extensions::api::usb::TransferType* output);
  bool ConvertUsageTypeSafely(const UsbUsageType& input,
                              extensions::api::usb::UsageType* output);

  scoped_ptr<base::ListValue> result_;
  scoped_ptr<extensions::api::usb::ListInterfaces::Params> parameters_;
};

class UsbCloseDeviceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.closeDevice", USB_CLOSEDEVICE)

  UsbCloseDeviceFunction();

 protected:
  virtual ~UsbCloseDeviceFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::CloseDevice::Params> parameters_;
};

class UsbClaimInterfaceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.claimInterface", USB_CLAIMINTERFACE)

  UsbClaimInterfaceFunction();

 protected:
  virtual ~UsbClaimInterfaceFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::ClaimInterface::Params> parameters_;
};

class UsbReleaseInterfaceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.releaseInterface", USB_RELEASEINTERFACE)

  UsbReleaseInterfaceFunction();

 protected:
  virtual ~UsbReleaseInterfaceFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::ReleaseInterface::Params> parameters_;
};

class UsbSetInterfaceAlternateSettingFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.setInterfaceAlternateSetting",
                             USB_SETINTERFACEALTERNATESETTING)

  UsbSetInterfaceAlternateSettingFunction();

 private:
  virtual ~UsbSetInterfaceAlternateSettingFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

  scoped_ptr<extensions::api::usb::SetInterfaceAlternateSetting::Params>
      parameters_;
};

class UsbControlTransferFunction : public UsbAsyncApiTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.controlTransfer", USB_CONTROLTRANSFER)

  UsbControlTransferFunction();

 protected:
  virtual ~UsbControlTransferFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::ControlTransfer::Params> parameters_;
};

class UsbBulkTransferFunction : public UsbAsyncApiTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.bulkTransfer", USB_BULKTRANSFER)

  UsbBulkTransferFunction();

 protected:
  virtual ~UsbBulkTransferFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::BulkTransfer::Params>
      parameters_;
};

class UsbInterruptTransferFunction : public UsbAsyncApiTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.interruptTransfer", USB_INTERRUPTTRANSFER)

  UsbInterruptTransferFunction();

 protected:
  virtual ~UsbInterruptTransferFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::InterruptTransfer::Params> parameters_;
};

class UsbIsochronousTransferFunction : public UsbAsyncApiTransferFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.isochronousTransfer", USB_ISOCHRONOUSTRANSFER)

  UsbIsochronousTransferFunction();

 protected:
  virtual ~UsbIsochronousTransferFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::IsochronousTransfer::Params> parameters_;
};

class UsbResetDeviceFunction : public UsbAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("usb.resetDevice", USB_RESETDEVICE)

  UsbResetDeviceFunction();

 protected:
  virtual ~UsbResetDeviceFunction();

  virtual bool Prepare() OVERRIDE;
  virtual void AsyncWorkStart() OVERRIDE;

 private:
  scoped_ptr<extensions::api::usb::ResetDevice::Params> parameters_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_API_H_
