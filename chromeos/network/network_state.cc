// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/strings/stringprintf.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_type_pattern.h"
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
  if (properties.GetStringWithoutPathExpansion(shill::kEapCaCertNssProperty,
                                               &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  const base::DictionaryValue* provider = NULL;
  properties.GetDictionaryWithoutPathExpansion(shill::kProviderProperty,
                                               &provider);
  if (!provider)
    return false;
  if (provider->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecCaCertNssProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }
  if (provider->GetStringWithoutPathExpansion(
          shill::kOpenVPNCaCertNSSProperty, &ca_cert_nss) &&
      !ca_cert_nss.empty()) {
    return true;
  }

  return false;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      visible_(false),
      connectable_(false),
      prefix_length_(0),
      signal_strength_(0),
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
  if (key == shill::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == shill::kStateProperty) {
    return GetStringValue(key, value, &connection_state_);
  } else if (key == shill::kVisibleProperty) {
    return GetBooleanValue(key, value, &visible_);
  } else if (key == shill::kConnectableProperty) {
    return GetBooleanValue(key, value, &connectable_);
  } else if (key == shill::kErrorProperty) {
    if (!GetStringValue(key, value, &error_))
      return false;
    if (ErrorIsValid(error_))
      last_error_ = error_;
    else
      error_.clear();
    return true;
  } else if (key == shill::kActivationTypeProperty) {
    return GetStringValue(key, value, &activation_type_);
  } else if (key == shill::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == shill::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == shill::kPaymentPortalProperty) {
    const base::DictionaryValue* olp;
    if (!value.GetAsDictionary(&olp))
      return false;
    return olp->GetStringWithoutPathExpansion(shill::kPaymentPortalURL,
                                              &payment_url_);
  } else if (key == shill::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == shill::kEapMethodProperty) {
    return GetStringValue(key, value, &eap_method_);
  } else if (key == shill::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == shill::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == shill::kProxyConfigProperty) {
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
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(
    const base::DictionaryValue& properties) {
  NET_LOG_DEBUG("InitialPropertiesReceived", path());
  bool changed = false;
  if (!properties.HasKey(shill::kTypeProperty)) {
    NET_LOG_ERROR("NetworkState has no type",
                  shill_property_util::GetNetworkIdFromProperties(properties));
    return false;
  }
  // Ensure that the network has a valid name.
  changed |= UpdateName(properties);

  // Set the has_ca_cert_nss_ property.
  bool had_ca_cert_nss = has_ca_cert_nss_;
  has_ca_cert_nss_ = IsCaCertNssSet(properties);
  changed |= had_ca_cert_nss != has_ca_cert_nss_;

  // By convention, all visible WiFi and WiMAX networks have a
  // SignalStrength > 0.
  if ((type() == shill::kTypeWifi || type() == shill::kTypeWimax) &&
      visible() && signal_strength_ <= 0) {
      signal_strength_ = 1;
  }

  return changed;
}

void NetworkState::GetStateProperties(base::DictionaryValue* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  // Properties shared by all types.
  dictionary->SetStringWithoutPathExpansion(shill::kGuidProperty, guid());
  dictionary->SetStringWithoutPathExpansion(shill::kSecurityProperty,
                                            security());

  if (visible()) {
    if (!error().empty())
      dictionary->SetStringWithoutPathExpansion(shill::kErrorProperty, error());
    dictionary->SetStringWithoutPathExpansion(shill::kStateProperty,
                                              connection_state());
  }

  // Wireless properties
  if (!NetworkTypePattern::Wireless().MatchesType(type()))
    return;

  if (visible()) {
    dictionary->SetBooleanWithoutPathExpansion(shill::kConnectableProperty,
                                               connectable());
    dictionary->SetIntegerWithoutPathExpansion(shill::kSignalStrengthProperty,
                                               signal_strength());
  }

  // Wifi properties
  if (NetworkTypePattern::WiFi().MatchesType(type())) {
    dictionary->SetStringWithoutPathExpansion(shill::kEapMethodProperty,
                                              eap_method());
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->SetStringWithoutPathExpansion(
        shill::kNetworkTechnologyProperty,
        network_technology());
    dictionary->SetStringWithoutPathExpansion(shill::kActivationStateProperty,
                                              activation_state());
    dictionary->SetStringWithoutPathExpansion(shill::kRoamingStateProperty,
                                              roaming());
    dictionary->SetBooleanWithoutPathExpansion(shill::kOutOfCreditsProperty,
                                               cellular_out_of_credits());
  }
}

void NetworkState::IPConfigPropertiesChanged(
    const base::DictionaryValue& properties) {
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    std::string key = iter.key();
    const base::Value& value = iter.value();

    if (key == shill::kAddressProperty) {
      GetStringValue(key, value, &ip_address_);
    } else if (key == shill::kGatewayProperty) {
      GetStringValue(key, value, &gateway_);
    } else if (key == shill::kNameServersProperty) {
      const base::ListValue* dns_servers;
      if (value.GetAsList(&dns_servers)) {
        dns_servers_.clear();
        ConvertListValueToStringVector(*dns_servers, &dns_servers_);
      }
    } else if (key == shill::kPrefixlenProperty) {
      GetIntegerValue(key, value, &prefix_length_);
    } else if (key == shill::kWebProxyAutoDiscoveryUrlProperty) {
      std::string url_string;
      if (GetStringValue(key, value, &url_string)) {
        if (url_string.empty()) {
          web_proxy_auto_discovery_url_ = GURL();
        } else {
          GURL gurl(url_string);
          if (gurl.is_valid()) {
            web_proxy_auto_discovery_url_ = gurl;
          } else {
            NET_LOG_ERROR("Invalid WebProxyAutoDiscoveryUrl: " + url_string,
                          path());
            web_proxy_auto_discovery_url_ = GURL();
          }
        }
      }
    }
  }
}

bool NetworkState::RequiresActivation() const {
  return (type() == shill::kTypeCellular &&
          activation_state() != shill::kActivationStateActivated &&
          activation_state() != shill::kActivationStateUnknown);
}

std::string NetworkState::connection_state() const {
  if (!visible())
    return shill::kStateDisconnect;
  return connection_state_;
}

bool NetworkState::IsConnectedState() const {
  return visible() && StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return visible() && StateIsConnecting(connection_state_);
}

bool NetworkState::IsInProfile() const {
  // kTypeEthernetEap is always saved. We need this check because it does
  // not show up in the visible list, but its properties may not be available
  // when it first shows up in ServiceCompleteList. See crbug.com/355117.
  return !profile_path_.empty() || type() == shill::kTypeEthernetEap;
}

bool NetworkState::IsPrivate() const {
  return !profile_path_.empty() &&
      profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
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

std::string NetworkState::GetSpecifier() const {
  if (!update_received()) {
    NET_LOG_ERROR("GetSpecifier called before update", path());
    return std::string();
  }
  if (type() == shill::kTypeWifi)
    return name() + "_" + security_;
  if (!name().empty())
    return name();
  return type();  // For unnamed networks such as ethernet.
}

void NetworkState::SetGuid(const std::string& guid) {
  guid_ = guid;
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
  return (connection_state == shill::kStateReady ||
          connection_state == shill::kStateOnline ||
          connection_state == shill::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& connection_state) {
  return (connection_state == shill::kStateAssociation ||
          connection_state == shill::kStateConfiguration ||
          connection_state == shill::kStateCarrier);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  // Shill uses "Unknown" to indicate an unset or cleared error state.
  return !error.empty() && error != kErrorUnknown;
}

}  // namespace chromeos
