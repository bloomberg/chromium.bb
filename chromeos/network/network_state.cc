// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
#include "chromeos/network/tether_constants.h"
#include "components/device_event_log/device_event_log.h"
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

bool IsCaptivePortalState(const base::DictionaryValue& properties, bool log) {
  std::string state;
  properties.GetStringWithoutPathExpansion(shill::kStateProperty, &state);
  if (state != shill::kStatePortal)
    return false;
  std::string portal_detection_phase, portal_detection_status;
  if (!properties.GetStringWithoutPathExpansion(
          shill::kPortalDetectionFailedPhaseProperty,
          &portal_detection_phase) ||
      !properties.GetStringWithoutPathExpansion(
          shill::kPortalDetectionFailedStatusProperty,
          &portal_detection_status)) {
    // If Shill (or a stub) has not set PortalDetectionFailedStatus
    // or PortalDetectionFailedPhase, assume we are in captive portal state.
    return true;
  }

  // Shill reports the phase in which it determined that the device is behind a
  // captive portal. We only want to rely only on incorrect content being
  // returned and ignore other reasons.
  bool is_captive_portal =
      portal_detection_phase == shill::kPortalDetectionPhaseContent &&
      portal_detection_status == shill::kPortalDetectionStatusFailure;

  if (log) {
    std::string name;
    properties.GetStringWithoutPathExpansion(shill::kNameProperty, &name);
    if (name.empty())
      properties.GetStringWithoutPathExpansion(shill::kSSIDProperty, &name);
    if (!is_captive_portal) {
      NET_LOG(EVENT) << "State is 'portal' but not in captive portal state:"
                     << " name=" << name << " phase=" << portal_detection_phase
                     << " status=" << portal_detection_status;
    } else {
      NET_LOG(EVENT) << "Network is in captive portal state: " << name;
    }
  }

  return is_captive_portal;
}

}  // namespace

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path) {}

NetworkState::~NetworkState() {}

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  // Keep care that these properties are the same as in |GetProperties|.
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == shill::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == shill::kStateProperty) {
    std::string saved_state = connection_state_;
    if (GetStringValue(key, value, &connection_state_)) {
      if (connection_state_ != saved_state)
        last_connection_state_ = saved_state;
      return true;
    } else {
      return false;
    }
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
  } else if (key == shill::kWifiFrequency) {
    return GetIntegerValue(key, value, &frequency_);
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
  } else if (key == shill::kSecurityClassProperty) {
    return GetStringValue(key, value, &security_class_);
  } else if (key == shill::kEapMethodProperty) {
    return GetStringValue(key, value, &eap_method_);
  } else if (key == shill::kEapKeyMgmtProperty) {
    return GetStringValue(key, value, &eap_key_mgmt_);
  } else if (key == shill::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &network_technology_);
  } else if (key == shill::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kWifiHexSsid) {
    std::string ssid_hex;
    if (!GetStringValue(key, value, &ssid_hex))
      return false;
    raw_ssid_.clear();
    return base::HexStringToBytes(ssid_hex, &raw_ssid_);
  } else if (key == shill::kWifiBSsid) {
    return GetStringValue(key, value, &bssid_);
  } else if (key == shill::kPriorityProperty) {
    return GetIntegerValue(key, value, &priority_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  } else if (key == shill::kProxyConfigProperty) {
    std::string proxy_config_str;
    if (!value.GetAsString(&proxy_config_str)) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }

    proxy_config_.Clear();
    if (proxy_config_str.empty())
      return true;

    std::unique_ptr<base::DictionaryValue> proxy_config_dict(
        onc::ReadDictionaryFromJson(proxy_config_str));
    if (proxy_config_dict) {
      // Warning: The DictionaryValue returned from
      // ReadDictionaryFromJson/JSONParser is an optimized derived class that
      // doesn't allow releasing ownership of nested values. A Swap in the wrong
      // order leads to memory access errors.
      proxy_config_.MergeDictionary(proxy_config_dict.get());
    } else {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
    }
    return true;
  } else if (key == shill::kProviderProperty) {
    std::string vpn_provider_type;
    const base::DictionaryValue* dict;
    if (!value.GetAsDictionary(&dict) ||
        !dict->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                             &vpn_provider_type)) {
      NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
      return false;
    }

    if (vpn_provider_type == shill::kProviderThirdPartyVpn) {
      // If the network uses a third-party VPN provider, copy over the
      // provider's extension ID, which is held in |shill::kHostProperty|.
      if (!dict->GetStringWithoutPathExpansion(
              shill::kHostProperty, &third_party_vpn_provider_extension_id_)) {
        NET_LOG(ERROR) << "Failed to parse " << path() << "." << key;
        return false;
      }
    } else {
      third_party_vpn_provider_extension_id_.clear();
    }

    vpn_provider_type_ = vpn_provider_type;
    return true;
  } else if (key == shill::kTetheringProperty) {
    return GetStringValue(key, value, &tethering_state_);
  }
  return false;
}

bool NetworkState::InitialPropertiesReceived(
    const base::DictionaryValue& properties) {
  NET_LOG(EVENT) << "InitialPropertiesReceived: " << path() << ": " << name()
                 << " State: " << connection_state_ << " Visible: " << visible_;
  if (!properties.HasKey(shill::kTypeProperty)) {
    NET_LOG(ERROR) << "NetworkState has no type: "
                   << shill_property_util::GetNetworkIdFromProperties(
                          properties);
    return false;
  }

  // By convention, all visible WiFi and WiMAX networks have a
  // SignalStrength > 0.
  if ((type() == shill::kTypeWifi || type() == shill::kTypeWimax) &&
      visible() && signal_strength_ <= 0) {
    signal_strength_ = 1;
  }

  // Any change to connection state will trigger a complete property update,
  // so we update is_captive_portal_ here.
  is_captive_portal_ = IsCaptivePortalState(properties, true /* log */);

  // Ensure that the network has a valid name.
  return UpdateName(properties);
}

void NetworkState::GetStateProperties(base::DictionaryValue* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  // Properties shared by all types.
  dictionary->SetStringWithoutPathExpansion(shill::kGuidProperty, guid());
  dictionary->SetStringWithoutPathExpansion(shill::kSecurityClassProperty,
                                            security_class());
  dictionary->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                            profile_path());
  dictionary->SetIntegerWithoutPathExpansion(shill::kPriorityProperty,
                                             priority_);

  if (visible()) {
    dictionary->SetStringWithoutPathExpansion(shill::kStateProperty,
                                              connection_state());
  }

  // VPN properties.
  if (NetworkTypePattern::VPN().MatchesType(type())) {
    // Shill sends VPN provider properties in a nested dictionary. |dictionary|
    // must replicate that nested structure.
    std::unique_ptr<base::DictionaryValue> provider_property(
        new base::DictionaryValue);
    provider_property->SetStringWithoutPathExpansion(shill::kTypeProperty,
                                                     vpn_provider_type_);
    if (vpn_provider_type_ == shill::kProviderThirdPartyVpn) {
      provider_property->SetStringWithoutPathExpansion(
          shill::kHostProperty, third_party_vpn_provider_extension_id_);
    }
    dictionary->SetWithoutPathExpansion(shill::kProviderProperty,
                                        std::move(provider_property));
  }

  // Tether properties
  if (NetworkTypePattern::Tether().MatchesType(type())) {
    dictionary->SetIntegerWithoutPathExpansion(kTetherBatteryPercentage,
                                               battery_percentage());
    dictionary->SetStringWithoutPathExpansion(kTetherCarrier, carrier());
    dictionary->SetBooleanWithoutPathExpansion(kTetherHasConnectedToHost,
                                               tether_has_connected_to_host());
    dictionary->SetIntegerWithoutPathExpansion(kTetherSignalStrength,
                                               signal_strength());
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
    dictionary->SetStringWithoutPathExpansion(shill::kWifiBSsid, bssid_);
    dictionary->SetStringWithoutPathExpansion(shill::kEapMethodProperty,
                                              eap_method());
    dictionary->SetIntegerWithoutPathExpansion(shill::kWifiFrequency,
                                               frequency_);
  }

  // Mobile properties
  if (NetworkTypePattern::Mobile().MatchesType(type())) {
    dictionary->SetStringWithoutPathExpansion(shill::kNetworkTechnologyProperty,
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
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
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
            NET_LOG(ERROR) << "Invalid WebProxyAutoDiscoveryUrl: " << path()
                           << ": " << url_string;
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

void NetworkState::set_connection_state(const std::string connection_state) {
  last_connection_state_ = connection_state_;
  connection_state_ = connection_state;
}

bool NetworkState::IsUsingMobileData() const {
  return type() == shill::kTypeCellular || type() == chromeos::kTypeTether ||
         tethering_state() == shill::kTetheringConfirmedState;
}

bool NetworkState::IsDynamicWep() const {
  return security_class_ == shill::kSecurityWep &&
         eap_key_mgmt_ == shill::kKeyManagementIEEE8021X;
}

bool NetworkState::IsConnectedState() const {
  return visible() && StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return visible() && StateIsConnecting(connection_state_);
}

bool NetworkState::IsReconnecting() const {
  return visible() && StateIsConnecting(connection_state_) &&
         StateIsConnected(last_connection_state_);
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

std::string NetworkState::GetHexSsid() const {
  return base::HexEncode(raw_ssid().data(), raw_ssid().size());
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
    NET_LOG(ERROR) << "GetSpecifier called before update: " << path();
    return std::string();
  }
  if (type() == shill::kTypeWifi)
    return name() + "_" + security_class_;
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

std::string NetworkState::GetErrorState() const {
  if (ErrorIsValid(error()))
    return error();
  return last_error();
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
bool NetworkState::NetworkStateIsCaptivePortal(
    const base::DictionaryValue& shill_properties) {
  return IsCaptivePortalState(shill_properties, false /* log */);
}

// static
bool NetworkState::ErrorIsValid(const std::string& error) {
  // Shill uses "Unknown" to indicate an unset or cleared error state.
  return !error.empty() && error != kErrorUnknown;
}

}  // namespace chromeos
