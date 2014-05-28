// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_type_pattern.h"

#include "chromeos/network/network_event_log.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kPatternDefault[] = "PatternDefault";
const char kPatternEthernet[] = "PatternEthernet";
const char kPatternWireless[] = "PatternWireless";
const char kPatternMobile[] = "PatternMobile";
const char kPatternNonVirtual[] = "PatternNonVirtual";

enum NetworkTypeBitFlag {
  kNetworkTypeNone = 0,
  kNetworkTypeEthernet = 1 << 0,
  kNetworkTypeWifi = 1 << 1,
  kNetworkTypeWimax = 1 << 2,
  kNetworkTypeCellular = 1 << 3,
  kNetworkTypeVPN = 1 << 4,
  kNetworkTypeEthernetEap = 1 << 5,
  kNetworkTypeBluetooth = 1 << 6
};

struct ShillToBitFlagEntry {
  const char* shill_network_type;
  NetworkTypeBitFlag bit_flag;
} shill_type_to_flag[] = {
  { shill::kTypeEthernet, kNetworkTypeEthernet },
  { shill::kTypeEthernetEap, kNetworkTypeEthernetEap },
  { shill::kTypeWifi, kNetworkTypeWifi },
  { shill::kTypeWimax, kNetworkTypeWimax },
  { shill::kTypeCellular, kNetworkTypeCellular },
  { shill::kTypeVPN, kNetworkTypeVPN },
  { shill::kTypeBluetooth, kNetworkTypeBluetooth }
};

NetworkTypeBitFlag ShillNetworkTypeToFlag(const std::string& shill_type) {
  for (size_t i = 0; i < arraysize(shill_type_to_flag); ++i) {
    if (shill_type_to_flag[i].shill_network_type == shill_type)
      return shill_type_to_flag[i].bit_flag;
  }
  NET_LOG_ERROR("ShillNetworkTypeToFlag", "Unknown type: " + shill_type);
  return kNetworkTypeNone;
}

}  // namespace

// static
NetworkTypePattern NetworkTypePattern::Default() {
  return NetworkTypePattern(~0);
}

// static
NetworkTypePattern NetworkTypePattern::Wireless() {
  return NetworkTypePattern(kNetworkTypeWifi | kNetworkTypeWimax |
                            kNetworkTypeCellular);
}

// static
NetworkTypePattern NetworkTypePattern::Mobile() {
  return NetworkTypePattern(kNetworkTypeCellular | kNetworkTypeWimax);
}

// static
NetworkTypePattern NetworkTypePattern::NonVirtual() {
  return NetworkTypePattern(~kNetworkTypeVPN);
}

// static
NetworkTypePattern NetworkTypePattern::Ethernet() {
  return NetworkTypePattern(kNetworkTypeEthernet);
}

// static
NetworkTypePattern NetworkTypePattern::WiFi() {
  return NetworkTypePattern(kNetworkTypeWifi);
}

// static
NetworkTypePattern NetworkTypePattern::Cellular() {
  return NetworkTypePattern(kNetworkTypeCellular);
}

// static
NetworkTypePattern NetworkTypePattern::VPN() {
  return NetworkTypePattern(kNetworkTypeVPN);
}

// static
NetworkTypePattern NetworkTypePattern::Wimax() {
  return NetworkTypePattern(kNetworkTypeWimax);
}

// static
NetworkTypePattern NetworkTypePattern::Primitive(
    const std::string& shill_network_type) {
  return NetworkTypePattern(ShillNetworkTypeToFlag(shill_network_type));
}

bool NetworkTypePattern::Equals(const NetworkTypePattern& other) const {
  return pattern_ == other.pattern_;
}

bool NetworkTypePattern::MatchesType(
    const std::string& shill_network_type) const {
  return MatchesPattern(Primitive(shill_network_type));
}

bool NetworkTypePattern::MatchesPattern(
    const NetworkTypePattern& other_pattern) const {
  if (Equals(other_pattern))
    return true;

  return pattern_ & other_pattern.pattern_;
}

std::string NetworkTypePattern::ToDebugString() const {
  if (Equals(Default()))
    return kPatternDefault;
  if (Equals(Ethernet()))
    return kPatternEthernet;
  if (Equals(Wireless()))
    return kPatternWireless;
  if (Equals(Mobile()))
    return kPatternMobile;
  if (Equals(NonVirtual()))
    return kPatternNonVirtual;

  std::string str;
  for (size_t i = 0; i < arraysize(shill_type_to_flag); ++i) {
    if (!(pattern_ & shill_type_to_flag[i].bit_flag))
      continue;
    if (!str.empty())
      str += "|";
    str += shill_type_to_flag[i].shill_network_type;
  }
  return str;
}

NetworkTypePattern::NetworkTypePattern(int pattern) : pattern_(pattern) {}

}  // namespace chromeos
