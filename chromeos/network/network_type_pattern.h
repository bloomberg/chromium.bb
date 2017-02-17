// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_TYPE_PATTERN_H_
#define CHROMEOS_NETWORK_NETWORK_TYPE_PATTERN_H_

#include <string>

#include "base/macros.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// A special case non-shill type.
CHROMEOS_EXPORT extern const char kTypeTether[];

class CHROMEOS_EXPORT NetworkTypePattern {
 public:
  // Matches any network.
  static NetworkTypePattern Default();

  // Matches wireless (WiFi, cellular, etc.) networks
  static NetworkTypePattern Wireless();

  // Matches cellular or wimax networks.
  static NetworkTypePattern Mobile();

  // Matches non virtual networks.
  static NetworkTypePattern NonVirtual();

  // Matches ethernet networks (with or without EAP).
  static NetworkTypePattern Ethernet();

  static NetworkTypePattern WiFi();
  static NetworkTypePattern Cellular();
  static NetworkTypePattern VPN();
  static NetworkTypePattern Wimax();

  static NetworkTypePattern Tether();

  // Matches only networks of exactly the type |shill_network_type|, which must
  // be one of the types defined in service_constants.h (e.g.
  // shill::kTypeWifi).
  // Note: Shill distinguishes Ethernet without EAP from Ethernet with EAP. If
  // unsure, better use one of the matchers above.
  static NetworkTypePattern Primitive(const std::string& shill_network_type);

  bool Equals(const NetworkTypePattern& other) const;
  bool MatchesType(const std::string& shill_network_type) const;

  // Returns true if this pattern matches at least one network type that
  // |other_pattern| matches (according to MatchesType). Thus MatchesPattern is
  // symmetric and reflexive but not transitive.
  // See the unit test for examples.
  bool MatchesPattern(const NetworkTypePattern& other_pattern) const;

  std::string ToDebugString() const;

 private:
  explicit NetworkTypePattern(int pattern);

  // The bit array of the matching network types.
  int pattern_;

  DISALLOW_ASSIGN(NetworkTypePattern);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_TYPE_PATTERN_H_
