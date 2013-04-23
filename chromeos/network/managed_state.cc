// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_state.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ManagedState::ManagedState(ManagedType type, const std::string& path)
    : managed_type_(type),
      path_(path),
      is_observed_(false) {
}

ManagedState::~ManagedState() {
}

ManagedState* ManagedState::Create(ManagedType type, const std::string& path) {
  switch(type) {
    case MANAGED_TYPE_NETWORK:
      return new NetworkState(path);
    case MANAGED_TYPE_DEVICE:
      return new DeviceState(path);
  }
  return NULL;
}

NetworkState* ManagedState::AsNetworkState() {
  if (managed_type() == MANAGED_TYPE_NETWORK)
    return static_cast<NetworkState*>(this);
  return NULL;
}

DeviceState* ManagedState::AsDeviceState() {
  if (managed_type() == MANAGED_TYPE_DEVICE)
    return static_cast<DeviceState*>(this);
  return NULL;
}

void ManagedState::InitialPropertiesReceived() {
}

bool ManagedState::ManagedStatePropertyChanged(const std::string& key,
                                               const base::Value& value) {
  if (key == flimflam::kNameProperty) {
    return GetStringValue(key, value, &name_);
  } else if (key == flimflam::kTypeProperty) {
    return GetStringValue(key, value, &type_);
  }
  return false;
}

bool ManagedState::GetBooleanValue(const std::string& key,
                                   const base::Value& value,
                                   bool* out_value) {
  bool res = value.GetAsBoolean(out_value);
  if (!res)
    LOG(WARNING) << "Failed to parse boolean value for:" << key;
  return res;
}

bool ManagedState::GetIntegerValue(const std::string& key,
                                   const base::Value& value,
                                   int* out_value) {
  bool res = value.GetAsInteger(out_value);
  if (!res)
    LOG(WARNING) << "Failed to parse integer value for:" << key;
  return res;
}

bool ManagedState::GetStringValue(const std::string& key,
                                  const base::Value& value,
                                  std::string* out_value) {
  bool res = value.GetAsString(out_value);
  if (!res)
    LOG(WARNING) << "Failed to parse string value for:" << key;
  return res;
}

}  // namespace chromeos
