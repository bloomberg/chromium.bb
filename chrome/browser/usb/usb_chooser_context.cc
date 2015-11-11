// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_chooser_context.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "device/core/device_client.h"
#include "device/usb/usb_device.h"

using device::UsbDevice;

namespace {

const char kDeviceNameKey[] = "name";
const char kProductIdKey[] = "product-id";
const char kSerialNumberKey[] = "serial-number";
const char kVendorIdKey[] = "vendor-id";

bool CanStorePersistentEntry(const scoped_refptr<const UsbDevice>& device) {
  return !device->serial_number().empty();
}

const base::DictionaryValue* FindForDevice(
    const ScopedVector<base::DictionaryValue>& device_list,
    const scoped_refptr<const UsbDevice>& device) {
  const std::string utf8_serial_number =
      base::UTF16ToUTF8(device->serial_number());

  for (const base::DictionaryValue* device_dict : device_list) {
    int vendor_id;
    int product_id;
    std::string serial_number;
    if (device_dict->GetInteger(kVendorIdKey, &vendor_id) &&
        device->vendor_id() == vendor_id &&
        device_dict->GetInteger(kProductIdKey, &product_id) &&
        device->product_id() == product_id &&
        device_dict->GetString(kSerialNumberKey, &serial_number) &&
        utf8_serial_number == serial_number) {
      return device_dict;
    }
  }
  return nullptr;
}

}  // namespace

UsbChooserContext::UsbChooserContext(Profile* profile)
    : ChooserContextBase(profile,
                         CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA),
      observer_(this) {
  usb_service_ = device::DeviceClient::Get()->GetUsbService();
  if (usb_service_)
    observer_.Add(usb_service_);
}

UsbChooserContext::~UsbChooserContext() {}

void UsbChooserContext::GrantDevicePermission(const GURL& requesting_origin,
                                              const GURL& embedding_origin,
                                              const std::string& guid) {
  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  if (CanStorePersistentEntry(device)) {
    scoped_ptr<base::DictionaryValue> device_dict(new base::DictionaryValue());
    device_dict->SetString(kDeviceNameKey, device->product_string());
    device_dict->SetInteger(kVendorIdKey, device->vendor_id());
    device_dict->SetInteger(kProductIdKey, device->product_id());
    device_dict->SetString(kSerialNumberKey, device->serial_number());
    GrantObjectPermission(requesting_origin, embedding_origin,
                          device_dict.Pass());
  } else {
    ephemeral_devices_[requesting_origin.GetOrigin()].insert(guid);
  }
}

void UsbChooserContext::RevokeDevicePermission(const GURL& requesting_origin,
                                               const GURL& embedding_origin,
                                               const std::string& guid) {
  auto it = ephemeral_devices_.find(requesting_origin.GetOrigin());
  if (it != ephemeral_devices_.end()) {
    it->second.erase(guid);
    if (it->second.empty())
      ephemeral_devices_.erase(it);
  }

  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return;

  ScopedVector<base::DictionaryValue> device_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  const base::DictionaryValue* entry = FindForDevice(device_list, device);
  if (entry)
    RevokeObjectPermission(requesting_origin, embedding_origin, *entry);
}

bool UsbChooserContext::HasDevicePermission(const GURL& requesting_origin,
                                            const GURL& embedding_origin,
                                            const std::string& guid) {
  auto it = ephemeral_devices_.find(requesting_origin.GetOrigin());
  if (it != ephemeral_devices_.end())
    return ContainsValue(it->second, guid);

  scoped_refptr<UsbDevice> device = usb_service_->GetDevice(guid);
  if (!device)
    return false;

  ScopedVector<base::DictionaryValue> device_list =
      GetGrantedObjects(requesting_origin, embedding_origin);
  return FindForDevice(device_list, device) != nullptr;
}

bool UsbChooserContext::IsValidObject(const base::DictionaryValue& object) {
  return object.size() == 4 && object.HasKey(kDeviceNameKey) &&
         object.HasKey(kVendorIdKey) && object.HasKey(kProductIdKey) &&
         object.HasKey(kSerialNumberKey);
}

void UsbChooserContext::OnDeviceRemoved(scoped_refptr<UsbDevice> device) {
  for (auto& map_entry : ephemeral_devices_)
    map_entry.second.erase(device->guid());
}
