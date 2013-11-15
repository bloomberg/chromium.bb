// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_SHILL_PROPERTY_UTIL_H_
#define CHROMEOS_NETWORK_SHILL_PROPERTY_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

class NetworkUIData;

namespace shill_property_util {

// Sets the |ssid| in |properties|.
CHROMEOS_EXPORT void SetSSID(const std::string ssid,
                             base::DictionaryValue* properties);

// Returns the SSID from |properties| in UTF-8 encoding. If |unknown_encoding|
// is not NULL, it is set to whether the SSID is of unknown encoding.
CHROMEOS_EXPORT std::string GetSSIDFromProperties(
    const base::DictionaryValue& properties,
    bool* unknown_encoding);

// Returns the name for the network represented by the Shill |properties|. For
// WiFi it refers to the HexSSID.
CHROMEOS_EXPORT std::string GetNameFromProperties(
    const std::string& service_path,
    const base::DictionaryValue& properties);

// Returns the UIData specified by |value|. Returns NULL if the value cannot be
// parsed.
scoped_ptr<NetworkUIData> GetUIDataFromValue(const base::Value& value);

// Returns the NetworkUIData parsed from the UIData property of
// |shill_dictionary|. If parsing fails or the field doesn't exist, returns
// NULL.
scoped_ptr<NetworkUIData> GetUIDataFromProperties(
    const base::DictionaryValue& shill_dictionary);

// Sets the UIData property in |shill_dictionary| to the serialization of
// |ui_data|.
void SetUIData(const NetworkUIData& ui_data,
               base::DictionaryValue* shill_dictionary);

// Copy configuration properties required by Shill to identify a network.
// Only WiFi, VPN, Ethernet and EthernetEAP are supported. WiMax and Cellular
// are not supported. Returns true only if all required properties could be
// copied.
bool CopyIdentifyingProperties(const base::DictionaryValue& service_properties,
                               base::DictionaryValue* dest);

// Compares the identifying configuration properties of |properties_a| and
// |properties_b|, returns true if they are identical. See also
// CopyIdentifyingProperties. Only WiFi, VPN, Ethernet and EthernetEAP are
// supported. WiMax and Cellular are not supported.
bool DoIdentifyingPropertiesMatch(const base::DictionaryValue& properties_a,
                                  const base::DictionaryValue& properties_b);

}  // namespace shill_property_util

class CHROMEOS_EXPORT NetworkTypePattern {
 public:
  // Matches any network.
  static NetworkTypePattern Default();

  // Matches wireless networks
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

#endif  // CHROMEOS_NETWORK_SHILL_PROPERTY_UTIL_H_
