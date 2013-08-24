// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/usb/usb_device_handle.h"
#include "chrome/common/extensions/api/usb.h"
#include "content/public/browser/browser_thread.h"

class UsbDeviceHandle;

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

// A UsbDeviceResource is an ApiResource wrapper for a UsbDevice.
class UsbDeviceResource : public ApiResource {
 public:
  UsbDeviceResource(const std::string& owner_extension_id,
                    scoped_refptr<UsbDeviceHandle> device);
  virtual ~UsbDeviceResource();

  scoped_refptr<UsbDeviceHandle> device() {
    return device_;
  }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::FILE;

 private:
  friend class ApiResourceManager<UsbDeviceResource>;
  static const char* service_name() {
    return "UsbDeviceResourceManager";
  }

  scoped_refptr<UsbDeviceHandle> device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_USB_USB_DEVICE_RESOURCE_H_
