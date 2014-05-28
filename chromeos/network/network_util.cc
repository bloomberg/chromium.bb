// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_util.h"

#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/network/favorite_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

WifiAccessPoint::WifiAccessPoint()
    : signal_strength(0),
      signal_to_noise(0),
      channel(0) {
}

WifiAccessPoint::~WifiAccessPoint() {
}

CellularScanResult::CellularScanResult() {
}

CellularScanResult::~CellularScanResult() {
}

namespace network_util {

std::string PrefixLengthToNetmask(int32 prefix_length) {
  std::string netmask;
  // Return the empty string for invalid inputs.
  if (prefix_length < 0 || prefix_length > 32)
    return netmask;
  for (int i = 0; i < 4; i++) {
    int remainder = 8;
    if (prefix_length >= 8) {
      prefix_length -= 8;
    } else {
      remainder = prefix_length;
      prefix_length = 0;
    }
    if (i > 0)
      netmask += ".";
    int value = remainder == 0 ? 0 :
        ((2L << (remainder - 1)) - 1) << (8 - remainder);
    netmask += base::StringPrintf("%d", value);
  }
  return netmask;
}

int32 NetmaskToPrefixLength(const std::string& netmask) {
  int count = 0;
  int prefix_length = 0;
  base::StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4)
      return -1;

    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefix_length / 8 != count) {
      if (token != "0")
        return -1;
    } else if (token == "255") {
      prefix_length += 8;
    } else if (token == "254") {
      prefix_length += 7;
    } else if (token == "252") {
      prefix_length += 6;
    } else if (token == "248") {
      prefix_length += 5;
    } else if (token == "240") {
      prefix_length += 4;
    } else if (token == "224") {
      prefix_length += 3;
    } else if (token == "192") {
      prefix_length += 2;
    } else if (token == "128") {
      prefix_length += 1;
    } else if (token == "0") {
      prefix_length += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefix_length;
}

std::string FormattedMacAddress(const std::string& shill_mac_address) {
  if (shill_mac_address.size() % 2 != 0)
    return shill_mac_address;
  std::string result;
  for (size_t i = 0; i < shill_mac_address.size(); ++i) {
    if ((i != 0) && (i % 2 == 0))
      result.push_back(':');
    result.push_back(base::ToUpperASCII(shill_mac_address[i]));
  }
  return result;
}

bool ParseCellularScanResults(const base::ListValue& list,
                              std::vector<CellularScanResult>* scan_results) {
  scan_results->clear();
  scan_results->reserve(list.GetSize());
  for (base::ListValue::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (!(*it)->IsType(base::Value::TYPE_DICTIONARY))
      return false;
    CellularScanResult scan_result;
    const base::DictionaryValue* dict =
        static_cast<const base::DictionaryValue*>(*it);
    // If the network id property is not present then this network cannot be
    // connected to so don't include it in the results.
    if (!dict->GetStringWithoutPathExpansion(shill::kNetworkIdProperty,
                                             &scan_result.network_id))
      continue;
    dict->GetStringWithoutPathExpansion(shill::kStatusProperty,
                                        &scan_result.status);
    dict->GetStringWithoutPathExpansion(shill::kLongNameProperty,
                                        &scan_result.long_name);
    dict->GetStringWithoutPathExpansion(shill::kShortNameProperty,
                                        &scan_result.short_name);
    dict->GetStringWithoutPathExpansion(shill::kTechnologyProperty,
                                        &scan_result.technology);
    scan_results->push_back(scan_result);
  }
  return true;
}

scoped_ptr<base::DictionaryValue> TranslateFavoriteStateToONC(
    const FavoriteState* favorite) {
  // Get the properties from the FavoriteState.
  base::DictionaryValue shill_dictionary;
  favorite->GetStateProperties(&shill_dictionary);

  // If a corresponding NetworkState exists, merge its State properties.
  const NetworkState* network_state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          favorite->path());
  if (network_state) {
    base::DictionaryValue shill_network_dictionary;
    network_state->GetStateProperties(&shill_network_dictionary);
    shill_dictionary.MergeDictionary(&shill_network_dictionary);
  }

  scoped_ptr<base::DictionaryValue> onc_dictionary =
      TranslateShillServiceToONCPart(
          shill_dictionary, &onc::kNetworkWithStateSignature);
  return onc_dictionary.Pass();
}

scoped_ptr<base::ListValue> TranslateNetworkListToONC(
    NetworkTypePattern pattern,
    bool configured_only,
    bool visible_only,
    int limit,
    bool debugging_properties) {
  NetworkStateHandler::FavoriteStateList favorite_states;
  NetworkHandler::Get()->network_state_handler()->GetFavoriteListByType(
      pattern, configured_only, visible_only, limit, &favorite_states);

  scoped_ptr<base::ListValue> network_properties_list(new base::ListValue);
  for (NetworkStateHandler::FavoriteStateList::iterator it =
           favorite_states.begin();
       it != favorite_states.end();
       ++it) {
    scoped_ptr<base::DictionaryValue> onc_dictionary =
        TranslateFavoriteStateToONC(*it);

    if (debugging_properties) {
      onc_dictionary->SetString("profile_path", (*it)->profile_path());
      std::string onc_source = (*it)->ui_data().GetONCSourceAsString();
      if (!onc_source.empty())
        onc_dictionary->SetString("onc_source", onc_source);
    }

    network_properties_list->Append(onc_dictionary.release());
  }
  return network_properties_list.Pass();
}

}  // namespace network_util
}  // namespace chromeos
