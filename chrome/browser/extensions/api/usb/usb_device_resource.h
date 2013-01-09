// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/usb/usb_device.h"
#include "chrome/common/extensions/api/usb.h"

class UsbDevice;

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

// A UsbDeviceResource is an ApiResource wrapper for a UsbDevice.
class UsbDeviceResource : public ApiResource {
 public:
  UsbDeviceResource(const std::string& owner_extension_id,
                    scoped_refptr<UsbDevice> device);
  virtual ~UsbDeviceResource();

  scoped_refptr<UsbDevice> device() {
    return device_;
  }

 private:
  scoped_refptr<UsbDevice> device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
