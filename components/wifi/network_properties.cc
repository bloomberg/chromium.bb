// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi/network_properties.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/onc/onc_constants.h"

namespace wifi {

NetworkProperties::NetworkProperties()
    : connection_state(onc::connection_state::kNotConnected),
      security(onc::wifi::kSecurityNone),
      signal_strength(0),
      auto_connect(false),
      frequency(kFrequencyUnknown) {
}

NetworkProperties::NetworkProperties(const NetworkProperties& other) = default;

NetworkProperties::~NetworkProperties() {
}

std::unique_ptr<base::DictionaryValue> NetworkProperties::ToValue(
    bool network_list) const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  value->SetString(onc::network_config::kGUID, guid);
  value->SetString(onc::network_config::kName, name);
  value->SetString(onc::network_config::kConnectionState, connection_state);
  DCHECK(type == onc::network_type::kWiFi);
  value->SetString(onc::network_config::kType, type);

  // For now, assume all WiFi services are connectable.
  value->SetBoolean(onc::network_config::kConnectable, true);

  std::unique_ptr<base::DictionaryValue> wifi(new base::DictionaryValue());
  wifi->SetString(onc::wifi::kSecurity, security);
  wifi->SetInteger(onc::wifi::kSignalStrength, signal_strength);

  // Network list expects subset of data.
  if (!network_list) {
    if (frequency != kFrequencyUnknown)
      wifi->SetInteger(onc::wifi::kFrequency, frequency);
    std::unique_ptr<base::ListValue> frequency_list(new base::ListValue());
    for (FrequencySet::const_iterator it = this->frequency_set.begin();
         it != this->frequency_set.end();
         ++it) {
      frequency_list->AppendInteger(*it);
    }
    if (!frequency_list->empty())
      wifi->Set(onc::wifi::kFrequencyList, std::move(frequency_list));
    if (!bssid.empty())
      wifi->SetString(onc::wifi::kBSSID, bssid);
    wifi->SetString(onc::wifi::kSSID, ssid);
    wifi->SetString(onc::wifi::kHexSSID,
                    base::HexEncode(ssid.c_str(), ssid.size()));
  }
  value->Set(onc::network_type::kWiFi, std::move(wifi));

  return value;
}

bool NetworkProperties::UpdateFromValue(const base::DictionaryValue& value) {
  const base::DictionaryValue* wifi = nullptr;
  std::string network_type;
  // Get network type and make sure that it is WiFi (if specified).
  if (value.GetString(onc::network_config::kType, &network_type)) {
    if (network_type != onc::network_type::kWiFi)
      return false;
    type = network_type;
  }
  if (value.GetDictionary(onc::network_type::kWiFi, &wifi)) {
    value.GetString(onc::network_config::kName, &name);
    value.GetString(onc::network_config::kGUID, &guid);
    value.GetString(onc::network_config::kConnectionState, &connection_state);
    wifi->GetString(onc::wifi::kSecurity, &security);
    wifi->GetString(onc::wifi::kSSID, &ssid);
    wifi->GetString(onc::wifi::kPassphrase, &password);
    wifi->GetBoolean(onc::wifi::kAutoConnect, &auto_connect);
    return true;
  }
  return false;
}

std::string NetworkProperties::MacAddressAsString(const uint8_t mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX:XX:XX:XX:XX:XX.
  static const char* const kMacFormatString = "%02x:%02x:%02x:%02x:%02x:%02x";
  return base::StringPrintf(kMacFormatString,
                            mac_as_int[0],
                            mac_as_int[1],
                            mac_as_int[2],
                            mac_as_int[3],
                            mac_as_int[4],
                            mac_as_int[5]);
}

bool NetworkProperties::OrderByType(const NetworkProperties& l,
                                    const NetworkProperties& r) {
  if (l.connection_state != r.connection_state)
    return l.connection_state < r.connection_state;
  // This sorting order is needed only for browser_tests, which expect this
  // network type sort order: ethernet < wifi < vpn < cellular.
  if (l.type == r.type)
    return l.guid < r.guid;
  if (l.type == onc::network_type::kEthernet)
    return true;
  if (r.type == onc::network_type::kEthernet)
    return false;
  return l.type > r.type;
}

}  // namespace wifi
