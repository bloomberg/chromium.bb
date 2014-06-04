// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_NETWORK_PROPERTIES_H_
#define COMPONENTS_WIFI_NETWORK_PROPERTIES_H_

#include <list>
#include <set>

#include "base/values.h"
#include "components/wifi/wifi_export.h"

namespace wifi {

typedef int32 Frequency;

enum FrequencyEnum {
  kFrequencyAny = 0,
  kFrequencyUnknown = 0,
  kFrequency2400 = 2400,
  kFrequency5000 = 5000
};

typedef std::set<Frequency> FrequencySet;

// Network Properties, can be used to parse the result of |GetProperties| and
// |GetVisibleNetworks|.
struct WIFI_EXPORT NetworkProperties {
  NetworkProperties();
  ~NetworkProperties();

  std::string connection_state;
  std::string guid;
  std::string name;
  std::string ssid;
  std::string bssid;
  std::string type;
  std::string security;
  // |password| field is used to pass wifi password for network creation via
  // |CreateNetwork| or connection via |StartConnect|. It does not persist
  // once operation is completed.
  std::string password;
  // WiFi Signal Strength. 0..100
  uint32 signal_strength;
  bool auto_connect;
  Frequency frequency;
  FrequencySet frequency_set;

  std::string json_extra;  // Extra JSON properties for unit tests

  scoped_ptr<base::DictionaryValue> ToValue(bool network_list) const;
  // Updates only properties set in |value|.
  bool UpdateFromValue(const base::DictionaryValue& value);
  static std::string MacAddressAsString(const uint8 mac_as_int[6]);
  static bool OrderByType(const NetworkProperties& l,
                          const NetworkProperties& r);
};

typedef std::list<NetworkProperties> NetworkList;

}  // namespace wifi

#endif  // COMPONENTS_WIFI_NETWORK_PROPERTIES_H_
