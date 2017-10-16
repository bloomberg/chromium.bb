// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/device_state.h"

#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

DeviceState::DeviceState(const std::string& path)
    : ManagedState(MANAGED_TYPE_DEVICE, path),
      allow_roaming_(false),
      provider_requires_roaming_(false),
      support_network_scan_(false),
      scanning_(false),
      sim_retries_left_(0),
      sim_present_(true),
      eap_authentication_completed_(false) {
}

DeviceState::~DeviceState() {
}

bool DeviceState::PropertyChanged(const std::string& key,
                                  const base::Value& value) {
  // All property values get stored in |properties_|.
  properties_.SetKey(key, value.Clone());

  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kAddressProperty) {
    return GetStringValue(key, value, &mac_address_);
  } else if (key == shill::kScanningProperty) {
    return GetBooleanValue(key, value, &scanning_);
  } else if (key == shill::kSupportNetworkScanProperty) {
    return GetBooleanValue(key, value, &support_network_scan_);
  } else if (key == shill::kCellularAllowRoamingProperty) {
    return GetBooleanValue(key, value, &allow_roaming_);
  } else if (key == shill::kProviderRequiresRoamingProperty) {
    return GetBooleanValue(key, value, &provider_requires_roaming_);
  } else if (key == shill::kHomeProviderProperty) {
    return shill_property_util::GetHomeProviderFromProperty(
        value, &home_provider_id_);
  } else if (key == shill::kTechnologyFamilyProperty) {
    return GetStringValue(key, value, &technology_family_);
  } else if (key == shill::kCarrierProperty) {
    return GetStringValue(key, value, &carrier_);
  } else if (key == shill::kFoundNetworksProperty) {
    const base::ListValue* list = nullptr;
    if (!value.GetAsList(&list))
      return false;
    CellularScanResults parsed_results;
    if (!network_util::ParseCellularScanResults(*list, &parsed_results))
      return false;
    scan_results_.swap(parsed_results);
    return true;
  } else if (key == shill::kSIMLockStatusProperty) {
    const base::DictionaryValue* dict = nullptr;
    if (!value.GetAsDictionary(&dict))
      return false;

    // Set default values for SIM properties.
    sim_lock_type_.erase();
    sim_retries_left_ = 0;
    sim_lock_enabled_ = false;

    const base::Value* out_value = nullptr;
    if (dict->GetWithoutPathExpansion(shill::kSIMLockTypeProperty,
                                      &out_value)) {
      GetStringValue(shill::kSIMLockTypeProperty, *out_value, &sim_lock_type_);
    }
    if (dict->GetWithoutPathExpansion(shill::kSIMLockRetriesLeftProperty,
                                      &out_value)) {
      GetIntegerValue(shill::kSIMLockRetriesLeftProperty, *out_value,
                      &sim_retries_left_);
    }
    if (dict->GetWithoutPathExpansion(shill::kSIMLockEnabledProperty,
                                      &out_value)) {
      GetBooleanValue(shill::kSIMLockEnabledProperty, *out_value,
                      &sim_lock_enabled_);
    }
    return true;
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
  } else if (key == shill::kIPConfigsProperty) {
    // If kIPConfigsProperty changes, clear any previous ip_configs_.
    // ShillPropertyhandler will request the IPConfig objects which will trigger
    // calls to IPConfigPropertiesChanged.
    ip_configs_.Clear();
    return false;  // No actual state change.
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

void DeviceState::IPConfigPropertiesChanged(
    const std::string& ip_config_path,
    const base::DictionaryValue& properties) {
  base::DictionaryValue* ip_config = nullptr;
  if (ip_configs_.GetDictionaryWithoutPathExpansion(
          ip_config_path, &ip_config)) {
    NET_LOG_EVENT("IPConfig Updated: " + ip_config_path, path());
    ip_config->Clear();
  } else {
    NET_LOG_EVENT("IPConfig Added: " + ip_config_path, path());
    ip_config = ip_configs_.SetDictionaryWithoutPathExpansion(
        ip_config_path, std::make_unique<base::DictionaryValue>());
  }
  ip_config->MergeDictionary(&properties);
}

std::string DeviceState::GetIpAddressByType(const std::string& type) const {
  for (base::DictionaryValue::Iterator iter(ip_configs_); !iter.IsAtEnd();
       iter.Advance()) {
    const base::DictionaryValue* ip_config;
    if (!iter.value().GetAsDictionary(&ip_config))
      continue;
    std::string ip_config_method;
    if (!ip_config->GetString(shill::kMethodProperty, &ip_config_method))
      continue;
    if (type == ip_config_method ||
        (type == shill::kTypeIPv4 && ip_config_method == shill::kTypeDHCP) ||
        (type == shill::kTypeIPv6 && ip_config_method == shill::kTypeDHCP6)) {
      std::string address;
      if (!ip_config->GetString(shill::kAddressProperty, &address))
        continue;
      return address;
    }
  }
  return std::string();
}

bool DeviceState::IsSimAbsent() const {
  return technology_family_ != shill::kTechnologyFamilyCdma && !sim_present_;
}

}  // namespace chromeos
