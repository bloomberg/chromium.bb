// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/geolocation_handler.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

GeolocationHandler::GeolocationHandler()
    : wifi_enabled_(false),
      weak_ptr_factory_(this) {
}

GeolocationHandler::~GeolocationHandler() {
  ShillManagerClient* manager_client =
      DBusThreadManager::Get()->GetShillManagerClient();
  if (manager_client)
    manager_client->RemovePropertyChangedObserver(this);
}

void GeolocationHandler::Init() {
  ShillManagerClient* manager_client =
      DBusThreadManager::Get()->GetShillManagerClient();
  manager_client->GetProperties(
      base::Bind(&GeolocationHandler::ManagerPropertiesCallback,
                 weak_ptr_factory_.GetWeakPtr()));
  manager_client->AddPropertyChangedObserver(this);
}

bool GeolocationHandler::GetWifiAccessPoints(
    WifiAccessPointVector* access_points, int64* age_ms) {
  if (!wifi_enabled_)
    return false;
  // Always request updated access points.
  RequestWifiAccessPoints();
  // If no data has been received, return false.
  if (geolocation_received_time_.is_null())
    return false;
  if (access_points)
    *access_points = wifi_access_points_;
  if (age_ms) {
    base::TimeDelta dtime = base::Time::Now() - geolocation_received_time_;
    *age_ms = dtime.InMilliseconds();
  }
  return true;
}

void GeolocationHandler::OnPropertyChanged(const std::string& key,
                                           const base::Value& value) {
  HandlePropertyChanged(key, value);
}

//------------------------------------------------------------------------------
// Private methods

void GeolocationHandler::ManagerPropertiesCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  const base::Value* value = NULL;
  if (properties.Get(shill::kEnabledTechnologiesProperty, &value) && value)
    HandlePropertyChanged(shill::kEnabledTechnologiesProperty, *value);
}

void GeolocationHandler::HandlePropertyChanged(const std::string& key,
                                               const base::Value& value) {
  if (key != shill::kEnabledTechnologiesProperty)
    return;
  const base::ListValue* technologies = NULL;
  if (!value.GetAsList(&technologies) || !technologies)
    return;
  bool wifi_was_enabled = wifi_enabled_;
  wifi_enabled_ = false;
  for (base::ListValue::const_iterator iter = technologies->begin();
       iter != technologies->end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    if (technology == shill::kTypeWifi) {
      wifi_enabled_ = true;
      break;
    }
  }
  if (!wifi_was_enabled && wifi_enabled_)
    RequestWifiAccessPoints();  // Request initial location data.
}

void GeolocationHandler::RequestWifiAccessPoints() {
  DBusThreadManager::Get()->GetShillManagerClient()->GetNetworksForGeolocation(
      base::Bind(&GeolocationHandler::GeolocationCallback,
                 weak_ptr_factory_.GetWeakPtr()));
}

void GeolocationHandler::GeolocationCallback(
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& properties) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    LOG(ERROR) << "Failed to get Geolocation data: " << call_status;
    return;
  }
  wifi_access_points_.clear();
  if (properties.empty())
    return;  // No enabled devices, don't update received time.

  // Dictionary<device_type, entry_list>
  for (base::DictionaryValue::Iterator iter(properties);
       !iter.IsAtEnd(); iter.Advance()) {
    const base::ListValue* entry_list = NULL;
    if (!iter.value().GetAsList(&entry_list)) {
      LOG(WARNING) << "Geolocation dictionary value not a List: " << iter.key();
      continue;
    }
    // List[Dictionary<key, value_str>]
    for (size_t i = 0; i < entry_list->GetSize(); ++i) {
      const base::DictionaryValue* entry = NULL;
      if (!entry_list->GetDictionary(i, &entry) || !entry) {
        LOG(WARNING) << "Geolocation list value not a Dictionary: " << i;
        continue;
      }
      // Docs: developers.google.com/maps/documentation/business/geolocation
      WifiAccessPoint wap;
      entry->GetString(shill::kGeoMacAddressProperty, &wap.mac_address);
      std::string age_str;
      if (entry->GetString(shill::kGeoAgeProperty, &age_str)) {
        int64 age_ms;
        if (base::StringToInt64(age_str, &age_ms)) {
          wap.timestamp =
              base::Time::Now() - base::TimeDelta::FromMilliseconds(age_ms);
        }
      }
      std::string strength_str;
      if (entry->GetString(shill::kGeoSignalStrengthProperty, &strength_str))
        base::StringToInt(strength_str, &wap.signal_strength);
      std::string signal_str;
      if (entry->GetString(shill::kGeoSignalToNoiseRatioProperty, &signal_str))
        base::StringToInt(signal_str, &wap.signal_to_noise);
      std::string channel_str;
      if (entry->GetString(shill::kGeoChannelProperty, &channel_str))
        base::StringToInt(channel_str, &wap.channel);
      wifi_access_points_.push_back(wap);
    }
  }
  geolocation_received_time_ = base::Time::Now();
}

}  // namespace chromeos
