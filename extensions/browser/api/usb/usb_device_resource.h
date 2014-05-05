// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_DEVICE_RESOURCE_H_
#define EXTENSIONS_BROWSER_API_USB_USB_DEVICE_RESOURCE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "components/usb_service/usb_device_handle.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/usb.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

// A UsbDeviceResource is an ApiResource wrapper for a UsbDevice.
class UsbDeviceResource : public ApiResource {
 public:
  UsbDeviceResource(const std::string& owner_extension_id,
                    scoped_refptr<usb_service::UsbDeviceHandle> device);
  virtual ~UsbDeviceResource();

  scoped_refptr<usb_service::UsbDeviceHandle> device() { return device_; }

  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::FILE;

 private:
  friend class ApiResourceManager<UsbDeviceResource>;
  static const char* service_name() { return "UsbDeviceResourceManager"; }

  scoped_refptr<usb_service::UsbDeviceHandle> device_;

  DISALLOW_COPY_AND_ASSIGN(UsbDeviceResource);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_DEVICE_RESOURCE_H_
