// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/common/extensions/api/experimental_usb.h"

class UsbDevice;

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

class APIResourceEventNotifier;

// A UsbDeviceResource is an APIResource wrapper for a UsbDevice. When invoking
// transfers on the underlying device it will use the APIResourceEventNotifier
// associated with the underlying APIResource to deliver completion messages.
class UsbDeviceResource : public APIResource {
 public:
  UsbDeviceResource(APIResourceEventNotifier* notifier, UsbDevice* device);
  virtual ~UsbDeviceResource();

  // All of the *Transfer variants that are exposed here adapt their arguments
  // for the underlying UsbDevice's interface and invoke the corresponding
  // methods with completion callbacks that call OnTransferComplete on the event
  // notifier.
  void ControlTransfer(
      const api::experimental_usb::ControlTransferInfo& transfer);
  void InterruptTransfer(
      const api::experimental_usb::GenericTransferInfo& transfer);
  void BulkTransfer(const api::experimental_usb::GenericTransferInfo& transfer);

 private:
  // Invoked by the underlying device's transfer callbacks. Indicates transfer
  // completion to the APIResource's event notifier.
  void TransferComplete(net::IOBuffer* buffer, const size_t length,
                        int success);

  scoped_refptr<UsbDevice> device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
