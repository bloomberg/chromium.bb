// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_RESOURCE_H_
#define CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_RESOURCE_H_

#include <string>

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_device_info.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace extensions {

class HidConnectionResource : public ApiResource {
 public:
  HidConnectionResource(const std::string& owner_extension_id,
                    scoped_refptr<device::HidConnection> connection);

  virtual ~HidConnectionResource();

  scoped_refptr<device::HidConnection> connection() {
    return connection_;
  }

 private:
  friend class ApiResourceManager<HidConnectionResource>;
  static const char* service_name() {
    return "HidDeviceResourceManager";
  }


  static const content::BrowserThread::ID kThreadId =
      content::BrowserThread::FILE;

  scoped_refptr<device::HidConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectionResource);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_RESOURCE_H_
