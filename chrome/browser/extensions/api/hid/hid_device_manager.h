// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/hid.h"
#include "device/hid/hid_device_info.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class HidDeviceManager : public BrowserContextKeyedAPI {
 public:
  explicit HidDeviceManager(content::BrowserContext* context);
  virtual ~HidDeviceManager();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<HidDeviceManager>* GetFactoryInstance();

  // Convenience method to get the HidDeviceManager for a profile.
  static HidDeviceManager* Get(content::BrowserContext* context) {
    return BrowserContextKeyedAPIFactory<HidDeviceManager>::Get(context);
  }

  scoped_ptr<base::ListValue> GetApiDevices(uint16_t vendor_id,
                                            uint16_t product_id);

  bool GetDeviceInfo(int resource_id, device::HidDeviceInfo* device_info);

 private:
  friend class BrowserContextKeyedAPIFactory<HidDeviceManager>;

  static const char* service_name() { return "HidDeviceManager"; }

  void UpdateDevices();

  base::ThreadChecker thread_checker_;

  int next_resource_id_;

  typedef std::map<int, device::HidDeviceId> ResourceIdToDeviceIdMap;
  typedef std::map<device::HidDeviceId, int> DeviceIdToResourceIdMap;

  ResourceIdToDeviceIdMap device_ids_;
  DeviceIdToResourceIdMap resource_ids_;

  DISALLOW_COPY_AND_ASSIGN(HidDeviceManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HID_HID_DEVICE_MANAGER_H_
