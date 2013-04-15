// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/device_state.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

DeviceState::DeviceState(const std::string& path)
    : ManagedState(MANAGED_TYPE_DEVICE, path),
      provider_requires_roaming_(false),
      support_network_scan_(false),
      scanning_(false),
      sim_present_(true) {
}

DeviceState::~DeviceState() {
}

bool DeviceState::PropertyChanged(const std::string& key,
                                  const base::Value& value) {
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == flimflam::kAddressProperty) {
    return GetStringValue(key, value, &mac_address_);
  } else if (key == flimflam::kScanningProperty) {
    return GetBooleanValue(key, value, &scanning_);
  } else if (key == flimflam::kSupportNetworkScanProperty) {
    return GetBooleanValue(key, value, &support_network_scan_);
  } else if (key == shill::kProviderRequiresRoamingProperty) {
    return GetBooleanValue(key, value, &provider_requires_roaming_);
  } else if (key == flimflam::kHomeProviderProperty) {
    const DictionaryValue* dict = NULL;
    if (!value.GetAsDictionary(&dict))
      return false;
    std::string home_provider_country;
    std::string home_provider_name;
    dict->GetStringWithoutPathExpansion(flimflam::kOperatorCountryKey,
                                        &home_provider_country);
    dict->GetStringWithoutPathExpansion(flimflam::kOperatorNameKey,
                                        &home_provider_name);
    // Set home_provider_id_
    if (!home_provider_name.empty() && !home_provider_country.empty()) {
      home_provider_id_ = base::StringPrintf(
          "%s (%s)",
          home_provider_name.c_str(),
          home_provider_country.c_str());
    } else {
      dict->GetStringWithoutPathExpansion(flimflam::kOperatorCodeKey,
                                          &home_provider_id_);
      LOG(WARNING) << "Carrier ID not defined, using code instead: "
                   << home_provider_id_;
    }
    return true;
  } else if (key == flimflam::kTechnologyFamilyProperty) {
    return GetStringValue(key, value, &technology_family_);
  } else if (key == flimflam::kSIMLockStatusProperty) {
    const DictionaryValue* dict = NULL;
    if (!value.GetAsDictionary(&dict))
      return false;
    if (!dict->GetStringWithoutPathExpansion(flimflam::kSIMLockTypeProperty,
                                             &sim_lock_type_))
      return false;
    // Ignore other SIMLockStatus properties.
    return true;
  } else if (key == shill::kSIMPresentProperty) {
    return GetBooleanValue(key, value, &sim_present_);
  }
  return false;
}

bool DeviceState::IsSimAbsent() const {
  return technology_family_ == flimflam::kTechnologyFamilyGsm && !sim_present_;
}

}  // namespace chromeos
