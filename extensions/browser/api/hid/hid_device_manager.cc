// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_device_manager.h"

#include <limits>
#include <vector>

#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"
#include "device/core/device_client.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"

using device::HidDeviceFilter;
using device::HidService;
using device::HidUsageAndPage;

namespace extensions {

HidDeviceManager::HidDeviceManager(content::BrowserContext* context)
    : next_resource_id_(0) {
}

HidDeviceManager::~HidDeviceManager() {}

// static
BrowserContextKeyedAPIFactory<HidDeviceManager>*
HidDeviceManager::GetFactoryInstance() {
  static base::LazyInstance<BrowserContextKeyedAPIFactory<HidDeviceManager> >
      factory = LAZY_INSTANCE_INITIALIZER;
  return &factory.Get();
}

scoped_ptr<base::ListValue> HidDeviceManager::GetApiDevices(
    const Extension* extension,
    const std::vector<HidDeviceFilter>& filters) {
  DCHECK(IsCalledOnValidThread());
  UpdateDevices();

  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
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
      if (!filters.empty() &&
          !HidDeviceFilter::MatchesAny(device_info, filters)) {
        continue;
      }

      if (!HasPermission(extension, device_info)) {
        continue;
      }

      core_api::hid::HidDeviceInfo api_device_info;
      api_device_info.device_id = resource_id;
      api_device_info.vendor_id = device_info.vendor_id;
      api_device_info.product_id = device_info.product_id;
      api_device_info.max_input_report_size = device_info.max_input_report_size;
      api_device_info.max_output_report_size =
          device_info.max_output_report_size;
      api_device_info.max_feature_report_size =
          device_info.max_feature_report_size;

      for (std::vector<device::HidCollectionInfo>::const_iterator
               collections_iter = device_info.collections.begin();
           collections_iter != device_info.collections.end();
           ++collections_iter) {
        const device::HidCollectionInfo& collection = *collections_iter;

        // Don't expose sensitive data.
        if (collection.usage.IsProtected()) {
          continue;
        }

        core_api::hid::HidCollectionInfo* api_collection =
            new core_api::hid::HidCollectionInfo();
        api_collection->usage_page = collection.usage.usage_page;
        api_collection->usage = collection.usage.usage;

        api_collection->report_ids.resize(collection.report_ids.size());
        std::copy(collection.report_ids.begin(),
                  collection.report_ids.end(),
                  api_collection->report_ids.begin());

        api_device_info.collections.push_back(make_linked_ptr(api_collection));
      }

      // Expose devices with which user can communicate.
      if (api_device_info.collections.size() > 0) {
        api_devices->Append(api_device_info.ToValue().release());
      }
    }
  }

  return scoped_ptr<base::ListValue>(api_devices);
}

bool HidDeviceManager::GetDeviceInfo(int resource_id,
                                     device::HidDeviceInfo* device_info) {
  DCHECK(IsCalledOnValidThread());
  UpdateDevices();
  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);

  ResourceIdToDeviceIdMap::const_iterator device_iter =
      device_ids_.find(resource_id);
  if (device_iter == device_ids_.end())
    return false;

  return hid_service->GetDeviceInfo(device_iter->second, device_info);
}

bool HidDeviceManager::HasPermission(const Extension* extension,
                                     const device::HidDeviceInfo& device_info) {
  DCHECK(IsCalledOnValidThread());
  UsbDevicePermission::CheckParam usbParam(
      device_info.vendor_id,
      device_info.product_id,
      UsbDevicePermissionData::UNSPECIFIED_INTERFACE);
  if (extension->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, &usbParam)) {
    return true;
  }

  if (extension->permissions_data()->HasAPIPermission(
          APIPermission::kU2fDevices)) {
    HidDeviceFilter u2f_filter;
    u2f_filter.SetUsagePage(0xF1D0);
    if (u2f_filter.Matches(device_info)) {
      return true;
    }
  }

  return false;
}

// static
bool HidDeviceManager::IsCalledOnValidThread() {
#if defined(OS_MACOSX)
  // Migration from FILE thread to UI thread. OS X gets it first.
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::UI);
#else
  // TODO(reillyg): Migrate Linux/CrOS and Windows as well.
  return content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE);
#endif
}

void HidDeviceManager::UpdateDevices() {
  DCHECK(IsCalledOnValidThread());
  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
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

}  // namespace extensions
