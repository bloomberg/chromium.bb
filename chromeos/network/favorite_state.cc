// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/favorite_state.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/shill_property_util.h"
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
  if (key == shill::kProfileProperty) {
    return GetStringValue(key, value, &profile_path_);
  } else if (key == shill::kUIDataProperty) {
    scoped_ptr<NetworkUIData> new_ui_data =
        shill_property_util::GetUIDataFromValue(value);
    if (!new_ui_data) {
      NET_LOG_ERROR("Failed to parse " + key, path());
      return false;
    }
    ui_data_ = *new_ui_data;
    return true;
  } else if (key == shill::kGuidProperty) {
    return GetStringValue(key, value, &guid_);
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
  } else if (key == shill::kSecurityProperty) {
    return GetStringValue(key, value, &security_);
  }
  return false;
}

void FavoriteState::GetStateProperties(
    base::DictionaryValue* dictionary) const {
  ManagedState::GetStateProperties(dictionary);

  dictionary->SetStringWithoutPathExpansion(shill::kGuidProperty, guid());
  dictionary->SetStringWithoutPathExpansion(shill::kProfileProperty,
                                            profile_path());
  // Add ONCSource for debugging.
  dictionary->SetStringWithoutPathExpansion(NetworkUIData::kKeyONCSource,
                                            ui_data_.GetONCSourceAsString());
}

std::string FavoriteState::GetSpecifier() const {
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

void FavoriteState::SetGuid(const std::string& guid) {
  DCHECK(guid_.empty());
  guid_ = guid;
}

bool FavoriteState::IsInProfile() const {
  // kTypeEthernetEap is always saved. We need this check because it does
  // not show up in the visible list, but its properties may not be available
  // when it first shows up in ServiceCompleteList. See crbug.com/355117.
  return !profile_path_.empty() || type() == shill::kTypeEthernetEap;
}

bool FavoriteState::IsPrivate() const {
  return !profile_path_.empty() &&
      profile_path_ != NetworkProfileHandler::GetSharedProfilePath();
}

}  // namespace chromeos
