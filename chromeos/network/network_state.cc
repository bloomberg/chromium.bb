// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state.h"

#include "base/values.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

NetworkState::NetworkState(const std::string& path)
    : ManagedState(MANAGED_TYPE_NETWORK, path),
      signal_strength_(0) {
}

NetworkState::~NetworkState() {
}

bool NetworkState::PropertyChanged(const std::string& key,
                                   const base::Value& value) {
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == flimflam::kSignalStrengthProperty) {
    return GetIntegerValue(key, value, &signal_strength_);
  } else if (key == flimflam::kStateProperty) {
    return GetStringValue(key, value, &state_);
  } else if (key == flimflam::kErrorProperty) {
    return GetStringValue(key, value, &error_);
  } else if (key == flimflam::kActivationStateProperty) {
    return GetStringValue(key, value, &activation_state_);
  } else if (key == flimflam::kRoamingStateProperty) {
    return GetStringValue(key, value, &roaming_);
  } else if (key == flimflam::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  } else if (key == flimflam::kNetworkTechnologyProperty) {
    return GetStringValue(key, value, &technology_);
  } else if (key == flimflam::kDeviceProperty) {
    return GetStringValue(key, value, &device_path_);
  }
  return false;
}

bool NetworkState::IsConnectedState() const {
  return StateIsConnected(state_);
}

bool NetworkState::IsConnectingState() const {
  return StateIsConnecting(state_);
}

// static
bool NetworkState::StateIsConnected(const std::string& state) {
  return (state == flimflam::kStateReady ||
          state == flimflam::kStateOnline ||
          state == flimflam::kStatePortal);
}

// static
bool NetworkState::StateIsConnecting(const std::string& state) {
  return (state == flimflam::kStateAssociation ||
          state == flimflam::kStateConfiguration ||
          state == flimflam::kStateCarrier);
}

}  // namespace chromeos
