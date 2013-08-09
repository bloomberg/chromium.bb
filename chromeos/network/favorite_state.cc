// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/favorite_state.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

FavoriteState::FavoriteState(const std::string& path)
    : ManagedState(MANAGED_TYPE_FAVORITE, path) {
}

FavoriteState::~FavoriteState() {
}

bool FavoriteState::PropertyChanged(const std::string& key,
                                    const base::Value& value) {
  if (ManagedStatePropertyChanged(key, value))
    return true;
  if (key == flimflam::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == flimflam::kUIDataProperty) {
    if (!NetworkState::GetUIDataFromValue(value, &ui_data_)) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }
    return true;
  }
  return false;
}

bool FavoriteState::IsManaged() const {
  return ui_data_.onc_source() == onc::ONC_SOURCE_DEVICE_POLICY ||
      ui_data_.onc_source() == onc::ONC_SOURCE_USER_POLICY;
}

bool FavoriteState::IsPrivate() const {
  return !profile_path_.empty() &&
      profile_path_ != NetworkProfileHandler::kSharedProfilePath;
}

}  // namespace chromeos
