// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_device_manager.h"

#include <limits>
#include <vector>

#include "base/lazy_instance.h"
#include "device/core/device_client.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_service.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"

namespace hid = extensions::core_api::hid;

using device::HidDeviceFilter;
using device::HidDeviceId;
using device::HidDeviceInfo;
using device::HidService;

namespace extensions {

namespace {

void PopulateHidDeviceInfo(hid::HidDeviceInfo* output,
                           scoped_refptr<HidDeviceInfo> input) {
  output->vendor_id = input->vendor_id();
  output->product_id = input->product_id();
  output->max_input_report_size = input->max_input_report_size();
  output->max_output_report_size = input->max_output_report_size();
  output->max_feature_report_size = input->max_feature_report_size();

  for (const device::HidCollectionInfo& collection : input->collections()) {
    // Don't expose sensitive data.
    if (collection.usage.IsProtected()) {
      continue;
    }

    hid::HidCollectionInfo* api_collection = new hid::HidCollectionInfo();
    api_collection->usage_page = collection.usage.usage_page;
    api_collection->usage = collection.usage.usage;

    api_collection->report_ids.resize(collection.report_ids.size());
    std::copy(collection.report_ids.begin(), collection.report_ids.end(),
              api_collection->report_ids.begin());

    output->collections.push_back(make_linked_ptr(api_collection));
  }

  const std::vector<uint8>& report_descriptor = input->report_descriptor();
  if (report_descriptor.size() > 0) {
    output->report_descriptor.assign(report_descriptor.begin(),
                                     report_descriptor.end());
  }
}

bool WillDispatchDeviceEvent(scoped_refptr<HidDeviceInfo> device_info,
                             content::BrowserContext* context,
                             const Extension* extension,
                             base::ListValue* event_args) {
  if (extension) {
    return HidDeviceManager::HasPermission(extension, device_info);
  }
  return false;
}

}  // namespace

struct HidDeviceManager::GetApiDevicesParams {
 public:
  GetApiDevicesParams(const Extension* extension,
                      const std::vector<HidDeviceFilter>& filters,
                      const GetApiDevicesCallback& callback)
      : extension(extension), filters(filters), callback(callback) {}
  ~GetApiDevicesParams() {}

  const Extension* extension;
  std::vector<HidDeviceFilter> filters;
  GetApiDevicesCallback callback;
};

HidDeviceManager::HidDeviceManager(content::BrowserContext* context)
    : initialized_(false),
      hid_service_observer_(this),
      enumeration_ready_(false),
      next_resource_id_(0),
      weak_factory_(this) {
  event_router_ = EventRouter::Get(context);
  if (event_router_) {
    event_router_->RegisterObserver(this, hid::OnDeviceAdded::kEventName);
    event_router_->RegisterObserver(this, hid::OnDeviceRemoved::kEventName);
  }
}

HidDeviceManager::~HidDeviceManager() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
BrowserContextKeyedAPIFactory<HidDeviceManager>*
HidDeviceManager::GetFactoryInstance() {
  static base::LazyInstance<BrowserContextKeyedAPIFactory<HidDeviceManager> >
      factory = LAZY_INSTANCE_INITIALIZER;
  return &factory.Get();
}

void HidDeviceManager::GetApiDevices(
    const Extension* extension,
    const std::vector<HidDeviceFilter>& filters,
    const GetApiDevicesCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LazyInitialize();

  if (enumeration_ready_) {
    scoped_ptr<base::ListValue> devices =
        CreateApiDeviceList(extension, filters);
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, base::Passed(&devices)));
  } else {
    pending_enumerations_.push_back(
        new GetApiDevicesParams(extension, filters, callback));
  }
}

scoped_refptr<HidDeviceInfo> HidDeviceManager::GetDeviceInfo(int resource_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);

  ResourceIdToDeviceIdMap::const_iterator device_iter =
      device_ids_.find(resource_id);
  if (device_iter == device_ids_.end()) {
    return nullptr;
  }

  return hid_service->GetDeviceInfo(device_iter->second);
}

// static
bool HidDeviceManager::HasPermission(const Extension* extension,
                                     scoped_refptr<HidDeviceInfo> device_info) {
  UsbDevicePermission::CheckParam usbParam(
      device_info->vendor_id(), device_info->product_id(),
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

void HidDeviceManager::Shutdown() {
  if (event_router_) {
    event_router_->UnregisterObserver(this);
  }
}

void HidDeviceManager::OnListenerAdded(const EventListenerInfo& details) {
  LazyInitialize();
}

void HidDeviceManager::OnDeviceAdded(scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LT(next_resource_id_, std::numeric_limits<int>::max());
  int new_id = next_resource_id_++;
  DCHECK(!ContainsKey(resource_ids_, device_info->device_id()));
  resource_ids_[device_info->device_id()] = new_id;
  device_ids_[new_id] = device_info->device_id();

  // Don't generate events during the initial enumeration.
  if (enumeration_ready_ && event_router_) {
    core_api::hid::HidDeviceInfo api_device_info;
    api_device_info.device_id = new_id;
    PopulateHidDeviceInfo(&api_device_info, device_info);

    if (api_device_info.collections.size() > 0) {
      scoped_ptr<base::ListValue> args(
          hid::OnDeviceAdded::Create(api_device_info));
      DispatchEvent(hid::OnDeviceAdded::kEventName, args.Pass(), device_info);
    }
  }
}

void HidDeviceManager::OnDeviceRemoved(
    scoped_refptr<HidDeviceInfo> device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto& resource_entry = resource_ids_.find(device_info->device_id());
  DCHECK(resource_entry != resource_ids_.end());
  int resource_id = resource_entry->second;
  const auto& device_entry = device_ids_.find(resource_id);
  DCHECK(device_entry != device_ids_.end());
  resource_ids_.erase(resource_entry);
  device_ids_.erase(device_entry);

  if (event_router_) {
    DCHECK(enumeration_ready_);
    scoped_ptr<base::ListValue> args(hid::OnDeviceRemoved::Create(resource_id));
    DispatchEvent(hid::OnDeviceRemoved::kEventName, args.Pass(), device_info);
  }
}

void HidDeviceManager::LazyInitialize() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (initialized_) {
    return;
  }

  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);
  hid_service->GetDevices(base::Bind(&HidDeviceManager::OnEnumerationComplete,
                                     weak_factory_.GetWeakPtr()));

  hid_service_observer_.Add(hid_service);
  initialized_ = true;
}

scoped_ptr<base::ListValue> HidDeviceManager::CreateApiDeviceList(
    const Extension* extension,
    const std::vector<HidDeviceFilter>& filters) {
  HidService* hid_service = device::DeviceClient::Get()->GetHidService();
  DCHECK(hid_service);

  scoped_ptr<base::ListValue> api_devices(new base::ListValue());
  for (const ResourceIdToDeviceIdMap::value_type& map_entry : device_ids_) {
    int resource_id = map_entry.first;
    const HidDeviceId& device_id = map_entry.second;

    scoped_refptr<HidDeviceInfo> device_info =
        hid_service->GetDeviceInfo(device_id);
    if (!device_info) {
      continue;
    }

    if (!filters.empty() &&
        !HidDeviceFilter::MatchesAny(device_info, filters)) {
      continue;
    }

    if (!HasPermission(extension, device_info)) {
      continue;
    }

    hid::HidDeviceInfo api_device_info;
    api_device_info.device_id = resource_id;
    PopulateHidDeviceInfo(&api_device_info, device_info);

    // Expose devices with which user can communicate.
    if (api_device_info.collections.size() > 0) {
      api_devices->Append(api_device_info.ToValue().release());
    }
  }

  return api_devices.Pass();
}

void HidDeviceManager::OnEnumerationComplete(
    const std::vector<scoped_refptr<HidDeviceInfo>>& devices) {
  DCHECK(resource_ids_.empty());
  DCHECK(device_ids_.empty());
  for (const scoped_refptr<HidDeviceInfo>& device_info : devices) {
    OnDeviceAdded(device_info);
  }
  enumeration_ready_ = true;

  for (const GetApiDevicesParams* params : pending_enumerations_) {
    scoped_ptr<base::ListValue> devices =
        CreateApiDeviceList(params->extension, params->filters);
    params->callback.Run(devices.Pass());
  }
  pending_enumerations_.clear();
}

void HidDeviceManager::DispatchEvent(const std::string& event_name,
                                     scoped_ptr<base::ListValue> event_args,
                                     scoped_refptr<HidDeviceInfo> device_info) {
  scoped_ptr<Event> event(new Event(event_name, event_args.Pass()));
  event->will_dispatch_callback =
      base::Bind(&WillDispatchDeviceEvent, device_info);
  event_router_->BroadcastEvent(event.Pass());
}

}  // namespace extensions
