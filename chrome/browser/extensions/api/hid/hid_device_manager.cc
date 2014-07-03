// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_device_manager.h"

#include <limits>
#include <vector>

#include "base/lazy_instance.h"
#include "device/hid/hid_service.h"

using device::HidService;
using device::HidUsageAndPage;

namespace extensions {

HidDeviceManager::HidDeviceManager(content::BrowserContext* context)
  : next_resource_id_(0) {}

HidDeviceManager::~HidDeviceManager() {}

// static
BrowserContextKeyedAPIFactory<HidDeviceManager>*
HidDeviceManager::GetFactoryInstance() {
  static base::LazyInstance<BrowserContextKeyedAPIFactory<HidDeviceManager> >
      factory = LAZY_INSTANCE_INITIALIZER;
  return &factory.Get();
}

scoped_ptr<base::ListValue> HidDeviceManager::GetApiDevices(
    uint16_t vendor_id,
    uint16_t product_id) {
  UpdateDevices();

  HidService* hid_service = HidService::GetInstance();
  DCHECK(hid_service);
  base::ListValue* api_devices = new base::ListValue();
  for (ResourceIdToDeviceIdMap::const_iterator device_iter =
           device_ids_.begin();
       device_iter != device_ids_.end();
       ++device_iter) {
    int resource_id = device_iter->first;
    device::HidDeviceId device_id = device_iter->second;
    device::HidDeviceInfo device_info;
    if (hid_service->GetDeviceInfo(device_id, &device_info)) {
      if (device_info.vendor_id == vendor_id &&
          device_info.product_id == product_id &&
          IsDeviceAccessible(device_info)) {
        api::hid::HidDeviceInfo api_device_info;
        api_device_info.device_id = resource_id;
        api_device_info.vendor_id = device_info.vendor_id;
        api_device_info.product_id = device_info.product_id;
        for (std::vector<device::HidUsageAndPage>::const_iterator usage_iter =
                 device_info.usages.begin();
             usage_iter != device_info.usages.end();
             ++usage_iter) {
          api::hid::HidUsageAndPage* usage_and_page =
              new api::hid::HidUsageAndPage();
          usage_and_page->usage_page = (*usage_iter).usage_page;
          usage_and_page->usage = (*usage_iter).usage;
          linked_ptr<api::hid::HidUsageAndPage> usage_and_page_ptr(
              usage_and_page);
          api_device_info.usages.push_back(usage_and_page_ptr);
        }
        api_devices->Append(api_device_info.ToValue().release());
      }
    }
  }
  return scoped_ptr<base::ListValue>(api_devices);
}

bool HidDeviceManager::GetDeviceInfo(int resource_id,
                                     device::HidDeviceInfo* device_info) {
  UpdateDevices();
  HidService* hid_service = HidService::GetInstance();
  DCHECK(hid_service);

  ResourceIdToDeviceIdMap::const_iterator device_iter =
      device_ids_.find(resource_id);
  if (device_iter == device_ids_.end())
    return false;

  return hid_service->GetDeviceInfo(device_iter->second, device_info);
}

void HidDeviceManager::UpdateDevices() {
  thread_checker_.CalledOnValidThread();
  HidService* hid_service = HidService::GetInstance();
  DCHECK(hid_service);

  std::vector<device::HidDeviceInfo> devices;
  hid_service->GetDevices(&devices);

  // Build an updated bidi mapping between resource ID and underlying device ID.
  DeviceIdToResourceIdMap new_resource_ids;
  ResourceIdToDeviceIdMap new_device_ids;
  for (std::vector<device::HidDeviceInfo>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    const device::HidDeviceInfo& device_info = *iter;
    DeviceIdToResourceIdMap::iterator resource_iter =
        resource_ids_.find(device_info.device_id);
    int new_id;
    if (resource_iter != resource_ids_.end()) {
      new_id = resource_iter->second;
    } else {
      DCHECK_LT(next_resource_id_, std::numeric_limits<int>::max());
      new_id = next_resource_id_++;
    }
    new_resource_ids[device_info.device_id] = new_id;
    new_device_ids[new_id] = device_info.device_id;
  }
  device_ids_.swap(new_device_ids);
  resource_ids_.swap(new_resource_ids);
}

// static
// TODO(rockot): Add some tests for this.
bool HidDeviceManager::IsDeviceAccessible(
    const device::HidDeviceInfo& device_info) {
  for (std::vector<device::HidUsageAndPage>::const_iterator iter =
           device_info.usages.begin();
      iter != device_info.usages.end(); ++iter) {
    if (!IsUsageAccessible(*iter)) {
      return false;
    }
  }
  return true;
}

// static
bool HidDeviceManager::IsUsageAccessible(
    const HidUsageAndPage& usage_and_page) {
  if (usage_and_page.usage_page == HidUsageAndPage::kPageKeyboard)
    return false;

  if (usage_and_page.usage_page != HidUsageAndPage::kPageGenericDesktop)
    return true;

  uint16_t usage = usage_and_page.usage;
  if (usage == HidUsageAndPage::kGenericDesktopPointer ||
      usage == HidUsageAndPage::kGenericDesktopMouse ||
      usage == HidUsageAndPage::kGenericDesktopKeyboard ||
      usage == HidUsageAndPage::kGenericDesktopKeypad) {
    return false;
  }

  if (usage >= HidUsageAndPage::kGenericDesktopSystemControl &&
      usage <= HidUsageAndPage::kGenericDesktopSystemWarmRestart) {
    return false;
  }

  if (usage >= HidUsageAndPage::kGenericDesktopSystemDock &&
      usage <= HidUsageAndPage::kGenericDesktopSystemDisplaySwap) {
    return false;
  }

  return true;
}

}  // namespace extensions
