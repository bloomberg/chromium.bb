// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include <string>
#include <vector>

#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "device/usb/public/mojom/device_manager.mojom.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefUrlsKey[] = "urls";
constexpr char kPrefVendorIdKey[] = "vendor_id";
constexpr char kPrefProductIdKey[] = "product_id";

// Find the URL match by checking if a url pair in |url_set| matches the given
// GURL pair. An empty embedding URL signifies a wildcard, so ignore the
// embedding origin check for this case.
bool FindMatchInSet(const std::set<std::pair<GURL, GURL>>& url_set,
                    const GURL& requesting_origin,
                    const GURL& embedding_origin) {
  for (const auto& url_pair : url_set) {
    if (url_pair.first.GetOrigin() == requesting_origin.GetOrigin()) {
      if (url_pair.second.is_empty() ||
          url_pair.second.GetOrigin() == embedding_origin.GetOrigin()) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

UsbPolicyAllowedDevices::UsbPolicyAllowedDevices(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  // Add an observer for |kManagedWebUsbAllowDevicesForUrls| to call
  // CreateOrUpdateMap when the value is changed. The lifetime of
  // |pref_change_registrar_| is managed by this class, therefore it is safe to
  // use base::Unretained here.
  pref_change_registrar_.Add(
      prefs::kManagedWebUsbAllowDevicesForUrls,
      base::BindRepeating(&UsbPolicyAllowedDevices::CreateOrUpdateMap,
                          base::Unretained(this)));

  CreateOrUpdateMap();
}

UsbPolicyAllowedDevices::~UsbPolicyAllowedDevices() {}

bool UsbPolicyAllowedDevices::IsDeviceAllowed(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  // Search through each set of URL pair that match the given device. The
  // keys correspond to the following URL pair sets:
  //  * (vendor_id, product_id): A set corresponding to the exact device.
  //  * (vendor_id, -1): A set corresponding to any device with |vendor_id|.
  //  * (-1, -1): A set corresponding to any device.
  const std::pair<int, int> set_keys[] = {
      std::make_pair(device_info.vendor_id, device_info.product_id),
      std::make_pair(device_info.vendor_id, -1), std::make_pair(-1, -1)};

  for (const auto& key : set_keys) {
    const auto entry = usb_device_ids_to_urls_.find(key);
    if (entry == usb_device_ids_to_urls_.cend())
      continue;

    if (FindMatchInSet(entry->second, requesting_origin, embedding_origin))
      return true;
  }
  return false;
}

void UsbPolicyAllowedDevices::CreateOrUpdateMap() {
  const base::Value* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedWebUsbAllowDevicesForUrls);
  usb_device_ids_to_urls_.clear();

  // A policy has not been assigned.
  if (!pref_value) {
    return;
  }

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetList()) {
    const base::Value* urls_list = item.FindKey(kPrefUrlsKey);
    std::set<std::pair<GURL, GURL>> parsed_url_set;

    // A urls item can contain a pair of URLs that are delimited by a comma. If
    // it does not contain a second URL, set the embedding URL to an empty GURL
    // to signify a wildcard embedded URL.
    for (const auto& urls_value : urls_list->GetList()) {
      std::vector<std::string> urls =
          base::SplitString(urls_value.GetString(), ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);

      // Skip invalid URL entries.
      if (urls.empty())
        continue;

      GURL requesting_url(urls[0]);
      GURL embedding_url;
      if (urls.size() == 2 && !urls[1].empty())
        embedding_url = GURL(urls[1]);
      auto url_pair =
          std::make_pair(std::move(requesting_url), std::move(embedding_url));

      parsed_url_set.insert(std::move(url_pair));
    }

    // Ignore items with empty parsed URLs.
    if (parsed_url_set.empty())
      continue;

    // For each device entry in the map, create or update its respective URL
    // set.
    const base::Value* devices = item.FindKey(kPrefDevicesKey);
    for (const auto& device : devices->GetList()) {
      // A missing ID signifies a wildcard for that ID, so a sentinel value of
      // -1 is assigned.
      const base::Value* vendor_id_value = device.FindKey(kPrefVendorIdKey);
      const base::Value* product_id_value = device.FindKey(kPrefProductIdKey);
      int vendor_id = vendor_id_value ? vendor_id_value->GetInt() : -1;
      int product_id = product_id_value ? product_id_value->GetInt() : -1;
      DCHECK(vendor_id != -1 || product_id == -1);

      auto key = std::make_pair(vendor_id, product_id);
      usb_device_ids_to_urls_[key].insert(parsed_url_set.begin(),
                                          parsed_url_set.end());
    }
  }
}
