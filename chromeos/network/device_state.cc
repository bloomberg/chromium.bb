// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/device_state.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

DeviceState::DeviceState(const std::string& path)
    : ManagedState(MANAGED_TYPE_DEVICE, path),
      provider_requires_roaming_(false),
      support_network_scan_(false),
      scanning_(false),
      sim_lock_enabled_(false),
      sim_present_(true),
      eap_authentication_completed_(false) {
}

DeviceState::~DeviceState() {
}

bool DeviceState::PropertyChanged(const std::string& key,
                                  const base::Value& value) {
  // All property values get stored in |properties_|.
  properties_.SetWithoutPathExpansion(key, value.DeepCopy());

  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kAddressProperty) {
    return GetStringValue(key, value, &mac_address_);
  } else if (key == shill::kScanningProperty) {
    return GetBooleanValue(key, value, &scanning_);
  } else if (key == shill::kSupportNetworkScanProperty) {
    return GetBooleanValue(key, value, &support_network_scan_);
  } else if (key == shill::kProviderRequiresRoamingProperty) {
    return GetBooleanValue(key, value, &provider_requires_roaming_);
  } else if (key == shill::kHomeProviderProperty) {
    const base::DictionaryValue* dict = NULL;
    if (!value.GetAsDictionary(&dict))
      return false;
    std::string home_provider_country;
    std::string home_provider_name;
    dict->GetStringWithoutPathExpansion(shill::kOperatorCountryKey,
                                        &home_provider_country);
    dict->GetStringWithoutPathExpansion(shill::kOperatorNameKey,
                                        &home_provider_name);
    // Set home_provider_id_
    if (!home_provider_name.empty() && !home_provider_country.empty()) {
      home_provider_id_ = base::StringPrintf(
          "%s (%s)",
          home_provider_name.c_str(),
          home_provider_country.c_str());
    } else {
      dict->GetStringWithoutPathExpansion(shill::kOperatorCodeKey,
                                          &home_provider_id_);
      LOG(WARNING) << "Carrier ID not defined, using code instead: "
                   << home_provider_id_;
    }
    return true;
  } else if (key == shill::kTechnologyFamilyProperty) {
    return GetStringValue(key, value, &technology_family_);
  } else if (key == shill::kCarrierProperty) {
    return GetStringValue(key, value, &carrier_);
  } else if (key == shill::kFoundNetworksProperty) {
    const base::ListValue* list = NULL;
    if (!value.GetAsList(&list))
      return false;
    CellularScanResults parsed_results;
    if (!network_util::ParseCellularScanResults(*list, &parsed_results))
      return false;
    scan_results_.swap(parsed_results);
    return true;
  } else if (key == shill::kSIMLockStatusProperty) {
    const base::DictionaryValue* dict = NULL;
    if (!value.GetAsDictionary(&dict))
      return false;

    // Return true if at least one of the property values changed.
    bool property_changed = false;
    const base::Value* out_value = NULL;
    if (!dict->GetWithoutPathExpansion(shill::kSIMLockRetriesLeftProperty,
                                       &out_value))
      return false;
    if (GetUInt32Value(shill::kSIMLockRetriesLeftProperty,
                       *out_value, &sim_retries_left_))
      property_changed = true;

    if (!dict->GetWithoutPathExpansion(shill::kSIMLockTypeProperty,
                                       &out_value))
      return false;
    if (GetStringValue(shill::kSIMLockTypeProperty,
                       *out_value, &sim_lock_type_))
      property_changed = true;

    if (!dict->GetWithoutPathExpansion(shill::kSIMLockEnabledProperty,
                                       &out_value))
      return false;
    if (GetBooleanValue(shill::kSIMLockEnabledProperty,
                        *out_value, &sim_lock_enabled_))
      property_changed = true;

    return property_changed;
  } else if (key == shill::kMeidProperty) {
    return GetStringValue(key, value, &meid_);
  } else if (key == shill::kImeiProperty) {
    return GetStringValue(key, value, &imei_);
  } else if (key == shill::kIccidProperty) {
    return GetStringValue(key, value, &iccid_);
  } else if (key == shill::kMdnProperty) {
    return GetStringValue(key, value, &mdn_);
  } else if (key == shill::kSIMPresentProperty) {
    return GetBooleanValue(key, value, &sim_present_);
  } else if (key == shill::kEapAuthenticationCompletedProperty) {
    return GetBooleanValue(key, value, &eap_authentication_completed_);
  }
  return false;
}

bool DeviceState::InitialPropertiesReceived(
    const base::DictionaryValue& properties) {
  // Update UMA stats.
  if (sim_present_) {
    bool locked = !sim_lock_type_.empty();
    UMA_HISTOGRAM_BOOLEAN("Cellular.SIMLocked", locked);
  }
  return false;
}

std::string DeviceState::GetFormattedMacAddress() const {
  if (mac_address_.size() % 2 != 0)
    return mac_address_;
  std::string result;
  for (size_t i = 0; i < mac_address_.size(); ++i) {
    if ((i != 0) && (i % 2 == 0))
      result.push_back(':');
    result.push_back(mac_address_[i]);
  }
  return result;
}

bool DeviceState::IsSimAbsent() const {
  return technology_family_ == shill::kTechnologyFamilyGsm && !sim_present_;
}

}  // namespace chromeos
