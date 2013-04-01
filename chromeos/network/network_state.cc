// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/values.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      auto_connect_(false),
      favorite_(false),
      priority_(0),
      signal_strength_(0),
      activate_over_non_cellular_networks_(false),
      cellular_out_of_credits_(false) {
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
  } else if (key == flimflam::kErrorProperty) {
    return GetStringValue(key, value, &error_);
  } else if (key == flimflam::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == flimflam::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == flimflam::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == flimflam::kAutoConnectProperty) {
    return GetBooleanValue(key, value, &auto_connect_);
  } else if (key == flimflam::kFavoriteProperty) {
    return GetBooleanValue(key, value, &favorite_);
  } else if (key == flimflam::kPriorityProperty) {
    return GetIntegerValue(key, value, &priority_);
  } else if (key == flimflam::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &technology_);
  } else if (key == flimflam::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  } else if (key == flimflam::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
  } else if (key == shill::kActivateOverNonCellularNetworkProperty) {
    return GetBooleanValue(key, value, &activate_over_non_cellular_networks_);
  } else if (key == shill::kOutOfCreditsProperty) {
    return GetBooleanValue(key, value, &cellular_out_of_credits_);
  }
  return false;
}

void NetworkState::GetProperties(base::DictionaryValue* dictionary) const {
  // Keep care that these properties are the same as in |PropertyChanged|.
  dictionary->SetStringWithoutPathExpansion(flimflam::kNameProperty, name());
  dictionary->SetStringWithoutPathExpansion(flimflam::kTypeProperty, type());
  dictionary->SetIntegerWithoutPathExpansion(flimflam::kSignalStrengthProperty,
                                             signal_strength());
  dictionary->SetStringWithoutPathExpansion(flimflam::kStateProperty,
                                            connection_state());
  dictionary->SetStringWithoutPathExpansion(flimflam::kErrorProperty,
                                            error());
  dictionary->SetStringWithoutPathExpansion(flimflam::kActivationStateProperty,
                                            activation_state());
  dictionary->SetStringWithoutPathExpansion(flimflam::kRoamingStateProperty,
                                            roaming());
  dictionary->SetStringWithoutPathExpansion(flimflam::kSecurityProperty,
                                            security());
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kAutoConnectProperty,
                                             auto_connect());
  dictionary->SetBooleanWithoutPathExpansion(flimflam::kFavoriteProperty,
                                             favorite());
  dictionary->SetIntegerWithoutPathExpansion(flimflam::kPriorityProperty,
                                             priority());
  dictionary->SetStringWithoutPathExpansion(
      flimflam::kNetworkTechnologyProperty,
      technology());
  dictionary->SetStringWithoutPathExpansion(flimflam::kDeviceProperty,
                                            device_path());
  dictionary->SetStringWithoutPathExpansion(flimflam::kGuidProperty, guid());
  dictionary->SetBooleanWithoutPathExpansion(
      shill::kActivateOverNonCellularNetworkProperty,
      activate_over_non_cellular_networks());
  dictionary->SetBooleanWithoutPathExpansion(shill::kOutOfCreditsProperty,
                                             cellular_out_of_credits());
}

bool NetworkState::IsConnectedState() const {
  return StateIsConnected(connection_state_);
}

bool NetworkState::IsConnectingState() const {
  return StateIsConnecting(connection_state_);
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

}  // namespace chromeos
