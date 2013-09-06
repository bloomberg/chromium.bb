// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/strings/stringprintf.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kErrorUnknown[] = "Unknown";

bool ConvertListValueToStringVector(const base::ListValue& string_list,
                                    std::vector<std::string>* result) {
  for (size_t i = 0; i < string_list.GetSize(); ++i) {
    std::string str;
    if (!string_list.GetString(i, &str))
      return false;
    result->push_back(str);
  }
  return true;
}

bool IsCaCertNssSet(const base::DictionaryValue& properties) {
  std::string ca_cert_nss;
  if (properties.GetStringWithoutPathExpansion(flimflam::kEapCaCertNssProperty,
                                               &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  const base::DictionaryValue* provider = NULL;
  properties.GetDictionaryWithoutPathExpansion(flimflam::kProviderProperty,
                                               &provider);
  if (!provider)
    return false;
  if (provider->GetStringWithoutPathExpansion(
          flimflam::kL2tpIpsecCaCertNssProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }
  if (provider->GetStringWithoutPathExpansion(
          flimflam::kOpenVPNCaCertNSSProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  return false;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      connectable_(false),
      prefix_length_(0),
      signal_strength_(0),
      activate_over_non_cellular_networks_(false),
      cellular_out_of_credits_(false),
      has_ca_cert_nss_(false) {
}

NetworkState::~NetworkState() {
}

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == flimflam::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == flimflam::kStateProperty) {
    return GetStringValue(key, value, &connection_state_);
  } else if (key == flimflam::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == flimflam::kErrorProperty) {
    if (!GetStringValue(key, value, &error_))
      return false;
    // Shill uses "Unknown" to indicate an unset error state.
    if (error_ == kErrorUnknown)
      error_.clear();
    return true;
  } else if (key == IPConfigProperty(flimflam::kAddressProperty)) {
    return GetStringValue(key, value, &ip_address_);
  } else if (key == IPConfigProperty(flimflam::kGatewayProperty)) {
    return GetStringValue(key, value, &gateway_);
  } else if (key == IPConfigProperty(flimflam::kNameServersProperty)) {
    const base::ListValue* dns_servers;
    if (!value.GetAsList(&dns_servers))
      return false;
    dns_servers_.clear();
    ConvertListValueToStringVector(*dns_servers, &dns_servers_);
    return true;
  } else if (key == IPConfigProperty(flimflam::kPrefixlenProperty)) {
    return GetIntegerValue(key, value, &prefix_length_);
  } else if (key == IPConfigProperty(
      shill::kWebProxyAutoDiscoveryUrlProperty)) {
    std::string url_string;
    if (!GetStringValue(key, value, &url_string))
      return false;
    if (url_string.empty()) {
      web_proxy_auto_discovery_url_ = GURL();
    } else {
      GURL gurl(url_string);
      if (!gurl.is_valid()) {
        web_proxy_auto_discovery_url_ = gurl;
      } else {
        NET_LOG_ERROR("Invalid WebProxyAutoDiscoveryUrl: " + url_string,
                      path());
        web_proxy_auto_discovery_url_ = GURL();
      }
    }
    return true;
  } else if (key == flimflam::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == flimflam::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == flimflam::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == flimflam::kProxyConfigProperty) {
    std::string proxy_config_str;
    if (!value.GetAsString(&proxy_config_str)) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }

    proxy_config_.Clear();
    if (proxy_config_str.empty())
      return true;

    scoped_ptr<base::DictionaryValue> proxy_config_dict(
        onc::ReadDictionaryFromJson(proxy_config_str));
    if (proxy_config_dict) {
      // Warning: The DictionaryValue returned from
      // ReadDictionaryFromJson/JSONParser is an optimized derived class that
      // doesn't allow releasing ownership of nested values. A Swap in the wrong
      // order leads to memory access errors.
      proxy_config_.MergeDictionary(proxy_config_dict.get());
    } else {
      NET_LOG_ERROR("Failed to parse " + key, path());
    }
    return true;
  } else if (key == flimflam::kUIDataProperty) {
    scoped_ptr<NetworkUIData> new_ui_data =
        shill_property_util::GetUIDataFromValue(value);
    if (!new_ui_data) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }
    ui_data_ = *new_ui_data;
    return true;
  } else if (key == flimflam::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == flimflam::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == flimflam::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == flimflam::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kActivateOverNonCellularNetworkProperty) {
    return GetBooleanValue(key, value, &activate_over_non_cellular_networks_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(
    const base::DictionaryValue& properties) {
  NET_LOG_DEBUG("InitialPropertiesReceived", path());
  bool changed = UpdateName(properties);
  bool had_ca_cert_nss = has_ca_cert_nss_;
  has_ca_cert_nss_ = IsCaCertNssSet(properties);
  changed |= had_ca_cert_nss != has_ca_cert_nss_;
  return changed;
}

void NetworkState::GetProperties(base::DictionaryValue* dictionary) const {
  // Keep care that these properties are the same as in |PropertyChanged|.
  dictionary->SetStringWithoutPathExpansion(flimflam::kNameProperty, name());
  dictionary->SetStringWithoutPathExpansion(flimflam::kTypeProperty, type());
  dictionary->SetIntegerWithoutPathExpansion(flimflam::kSignalStrengthProperty,
                                             signal_strength_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kStateProperty,
                                            connection_state_);
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kConnectableProperty,
                                             connectable_);

  dictionary->SetStringWithoutPathExpansion(flimflam::kErrorProperty,
                                            error_);

  // IPConfig properties
  base::DictionaryValue* ipconfig_properties = new base::DictionaryValue;
  ipconfig_properties->SetStringWithoutPathExpansion(flimflam::kAddressProperty,
                                                     ip_address_);
  ipconfig_properties->SetStringWithoutPathExpansion(flimflam::kGatewayProperty,
                                                     gateway_);
  base::ListValue* name_servers = new base::ListValue;
  name_servers->AppendStrings(dns_servers_);
  ipconfig_properties->SetWithoutPathExpansion(flimflam::kNameServersProperty,
                                               name_servers);
  ipconfig_properties->SetStringWithoutPathExpansion(
      shill::kWebProxyAutoDiscoveryUrlProperty,
      web_proxy_auto_discovery_url_.spec());
  dictionary->SetWithoutPathExpansion(shill::kIPConfigProperty,
                                      ipconfig_properties);

  dictionary->SetStringWithoutPathExpansion(flimflam::kActivationStateProperty,
                                            activation_state_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kRoamingStateProperty,
                                            roaming_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kSecurityProperty,
                                            security_);
  // Proxy config and ONC source are intentionally omitted: These properties are
  // placed in NetworkState to transition ProxyConfigServiceImpl from
  // NetworkLibrary to the new network stack. The networking extension API
  // shouldn't depend on this member. Once ManagedNetworkConfigurationHandler
  // is used instead of NetworkLibrary, we can remove them again.
  dictionary->SetStringWithoutPathExpansion(
      flimflam::kNetworkTechnologyProperty,
      network_technology_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kDeviceProperty,
                                            device_path_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kGuidProperty, guid_);
  dictionary->SetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                            profile_path_);
  dictionary->SetBooleanWithoutPathExpansion(
      shill::kActivateOverNonCellularNetworkProperty,
      activate_over_non_cellular_networks_);
  dictionary->SetBooleanWithoutPathExpansion(shill::kOutOfCreditsProperty,
                                             cellular_out_of_credits_);
}

bool NetworkState::IsConnectedState() const {
  return StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return StateIsConnecting(connection_state_);
}

bool NetworkState::IsManaged() const {
  return ui_data_.onc_source() == onc::ONC_SOURCE_DEVICE_POLICY ||
         ui_data_.onc_source() == onc::ONC_SOURCE_USER_POLICY;
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
      profile_path_ != NetworkProfileHandler::kSharedProfilePath;
}

std::string NetworkState::GetDnsServersAsString() const {
  std::string result;
  for (size_t i = 0; i < dns_servers_.size(); ++i) {
    if (i != 0)
      result += ",";
    result += dns_servers_[i];
  }
  return result;
}

std::string NetworkState::GetNetmask() const {
  return network_util::PrefixLengthToNetmask(prefix_length_);
}

bool NetworkState::UpdateName(const base::DictionaryValue& properties) {
  std::string updated_name =
      shill_property_util::GetNameFromProperties(path(), properties);
  if (updated_name != name()) {
    set_name(updated_name);
    return true;
  }
  return false;
}

// static
bool NetworkState::StateIsConnected(const std::string& connection_state) {
  return (connection_state == flimflam::kStateReady ||
          connection_state == flimflam::kStateOnline ||
          connection_state == flimflam::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == flimflam::kStateAssociation ||
          connection_state == flimflam::kStateConfiguration ||
          connection_state == flimflam::kStateCarrier);
}

// static
std::string NetworkState::IPConfigProperty(const char* key) {
  return base::StringPrintf("%s.%s", shill::kIPConfigProperty, key);
}

}  // namespace chromeos
