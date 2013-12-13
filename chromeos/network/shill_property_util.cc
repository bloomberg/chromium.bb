// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/shill_property_util.h"

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/values.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace shill_property_util {

namespace {

// Replace non UTF8 characters in |str| with a replacement character.
std::string ValidateUTF8(const std::string& str) {
  std::string result;
  for (int32 index = 0; index < static_cast<int32>(str.size()); ++index) {
    uint32 code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(
        str.c_str(), str.size(), &index, &code_point_out);
    const uint32 kFirstNonControlChar = 0x20;
    if (is_unicode_char && (code_point_out >= kFirstNonControlChar)) {
      base::WriteUnicodeCharacter(code_point_out, &result);
    } else {
      const uint32 kReplacementChar = 0xFFFD;
      // Puts kReplacementChar if character is a control character [0,0x20)
      // or is not readable UTF8.
      base::WriteUnicodeCharacter(kReplacementChar, &result);
    }
  }
  return result;
}

// If existent and non-empty, copies the string at |key| from |source| to
// |dest|. Returns true if the string was copied.
bool CopyStringFromDictionary(const base::DictionaryValue& source,
                              const std::string& key,
                              base::DictionaryValue* dest) {
  std::string string_value;
  if (!source.GetStringWithoutPathExpansion(key, &string_value) ||
      string_value.empty())
    return false;
  dest->SetStringWithoutPathExpansion(key, string_value);
  return true;
}

}  // namespace

void SetSSID(const std::string ssid, base::DictionaryValue* properties) {
  std::string hex_ssid = base::HexEncode(ssid.c_str(), ssid.size());
  properties->SetStringWithoutPathExpansion(shill::kWifiHexSsid, hex_ssid);
}

std::string GetSSIDFromProperties(const base::DictionaryValue& properties,
                                  bool* unknown_encoding) {
  if (unknown_encoding)
    *unknown_encoding = false;
  std::string hex_ssid;
  properties.GetStringWithoutPathExpansion(shill::kWifiHexSsid, &hex_ssid);

  if (hex_ssid.empty()) {
    NET_LOG_ERROR("GetSSIDFromProperties", "No HexSSID set.");
    return std::string();
  }

  std::string ssid;
  std::vector<uint8> raw_ssid_bytes;
  if (base::HexStringToBytes(hex_ssid, &raw_ssid_bytes)) {
    ssid = std::string(raw_ssid_bytes.begin(), raw_ssid_bytes.end());
    NET_LOG_DEBUG(
        "GetSSIDFromProperties",
        base::StringPrintf("%s, SSID: %s", hex_ssid.c_str(), ssid.c_str()));
  } else {
    NET_LOG_ERROR("GetSSIDFromProperties",
                  base::StringPrintf("Error processing: %s", hex_ssid.c_str()));
    return std::string();
  }

  if (IsStringUTF8(ssid))
    return ssid;

  // Detect encoding and convert to UTF-8.
  std::string encoding;
  if (!base::DetectEncoding(ssid, &encoding)) {
    // TODO(stevenjb): This is currently experimental. If we find a case where
    // base::DetectEncoding() fails, we need to figure out whether we can use
    // country_code with ConvertToUtf8(). crbug.com/233267.
    properties.GetStringWithoutPathExpansion(shill::kCountryProperty,
                                             &encoding);
  }
  std::string utf8_ssid;
  if (!encoding.empty() &&
      base::ConvertToUtf8AndNormalize(ssid, encoding, &utf8_ssid)) {
    if (utf8_ssid != ssid) {
      NET_LOG_DEBUG(
          "GetSSIDFromProperties",
          base::StringPrintf(
              "Encoding=%s: %s", encoding.c_str(), utf8_ssid.c_str()));
    }
    return utf8_ssid;
  }

  if (unknown_encoding)
    *unknown_encoding = true;
  NET_LOG_DEBUG(
      "GetSSIDFromProperties",
      base::StringPrintf("Unrecognized Encoding=%s", encoding.c_str()));
  return ssid;
}

std::string GetNameFromProperties(const std::string& service_path,
                                  const base::DictionaryValue& properties) {
  std::string name;
  properties.GetStringWithoutPathExpansion(shill::kNameProperty, &name);

  std::string validated_name = ValidateUTF8(name);
  if (validated_name != name) {
    NET_LOG_DEBUG("GetNameFromProperties",
                  base::StringPrintf("Validated name %s: UTF8: %s",
                                     service_path.c_str(),
                                     validated_name.c_str()));
  }

  std::string type;
  properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  if (!NetworkTypePattern::WiFi().MatchesType(type))
    return validated_name;

  bool unknown_ssid_encoding = false;
  std::string ssid = GetSSIDFromProperties(properties, &unknown_ssid_encoding);
  if (ssid.empty())
    NET_LOG_ERROR("GetNameFromProperties", "No SSID set: " + service_path);

  // Use |validated_name| if |ssid| is empty.
  // And if the encoding of the SSID is unknown, use |ssid|, which contains raw
  // bytes in that case, only if |validated_name| is empty.
  if (ssid.empty() || (unknown_ssid_encoding && !validated_name.empty()))
    return validated_name;

  if (ssid != validated_name) {
    NET_LOG_DEBUG("GetNameFromProperties",
                  base::StringPrintf("%s: SSID: %s, Name: %s",
                                     service_path.c_str(),
                                     ssid.c_str(),
                                     validated_name.c_str()));
  }
  return ssid;
}

scoped_ptr<NetworkUIData> GetUIDataFromValue(const base::Value& ui_data_value) {
  std::string ui_data_str;
  if (!ui_data_value.GetAsString(&ui_data_str))
    return scoped_ptr<NetworkUIData>();
  if (ui_data_str.empty())
    return make_scoped_ptr(new NetworkUIData());
  scoped_ptr<base::DictionaryValue> ui_data_dict(
      chromeos::onc::ReadDictionaryFromJson(ui_data_str));
  if (!ui_data_dict)
    return scoped_ptr<NetworkUIData>();
  return make_scoped_ptr(new NetworkUIData(*ui_data_dict));
}

scoped_ptr<NetworkUIData> GetUIDataFromProperties(
    const base::DictionaryValue& shill_dictionary) {
  const base::Value* ui_data_value = NULL;
  shill_dictionary.GetWithoutPathExpansion(shill::kUIDataProperty,
                                           &ui_data_value);
  if (!ui_data_value) {
    VLOG(2) << "Dictionary has no UIData entry.";
    return scoped_ptr<NetworkUIData>();
  }
  scoped_ptr<NetworkUIData> ui_data = GetUIDataFromValue(*ui_data_value);
  if (!ui_data)
    LOG(ERROR) << "UIData is not a valid JSON dictionary.";
  return ui_data.Pass();
}

void SetUIData(const NetworkUIData& ui_data,
               base::DictionaryValue* shill_dictionary) {
  base::DictionaryValue ui_data_dict;
  ui_data.FillDictionary(&ui_data_dict);
  std::string ui_data_blob;
  base::JSONWriter::Write(&ui_data_dict, &ui_data_blob);
  shill_dictionary->SetStringWithoutPathExpansion(shill::kUIDataProperty,
                                                  ui_data_blob);
}

bool CopyIdentifyingProperties(const base::DictionaryValue& service_properties,
                               base::DictionaryValue* dest) {
  bool success = true;

  // GUID is optional.
  CopyStringFromDictionary(service_properties, shill::kGuidProperty, dest);

  std::string type;
  service_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  success &= !type.empty();
  dest->SetStringWithoutPathExpansion(shill::kTypeProperty, type);
  if (type == shill::kTypeWifi) {
    success &= CopyStringFromDictionary(
        service_properties, shill::kSecurityProperty, dest);
    success &=
        CopyStringFromDictionary(service_properties, shill::kWifiHexSsid, dest);
    success &= CopyStringFromDictionary(
        service_properties, shill::kModeProperty, dest);
  } else if (type == shill::kTypeVPN) {
    success &= CopyStringFromDictionary(
        service_properties, shill::kNameProperty, dest);
    // VPN Provider values are read from the "Provider" dictionary, but written
    // with the keys "Provider.Type" and "Provider.Host".
    const base::DictionaryValue* provider_properties = NULL;
    if (!service_properties.GetDictionaryWithoutPathExpansion(
             shill::kProviderProperty, &provider_properties)) {
      NET_LOG_ERROR("CopyIdentifyingProperties", "Missing VPN provider dict");
      return false;
    }
    std::string vpn_provider_type;
    provider_properties->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                       &vpn_provider_type);
    success &= !vpn_provider_type.empty();
    dest->SetStringWithoutPathExpansion(shill::kProviderTypeProperty,
                                        vpn_provider_type);

    std::string vpn_provider_host;
    provider_properties->GetStringWithoutPathExpansion(shill::kHostProperty,
                                                       &vpn_provider_host);
    success &= !vpn_provider_host.empty();
    dest->SetStringWithoutPathExpansion(shill::kProviderHostProperty,
                                        vpn_provider_host);
  } else if (type == shill::kTypeEthernet || type == shill::kTypeEthernetEap) {
    // Ethernet and EthernetEAP don't have any additional identifying
    // properties.
  } else {
    NOTREACHED() << "Unsupported network type " << type;
    success = false;
  }
  if (!success)
    NET_LOG_ERROR("CopyIdentifyingProperties", "Missing required properties");
  return success;
}

bool DoIdentifyingPropertiesMatch(const base::DictionaryValue& properties_a,
                                  const base::DictionaryValue& properties_b) {
  base::DictionaryValue identifying_a;
  if (!CopyIdentifyingProperties(properties_a, &identifying_a))
    return false;
  base::DictionaryValue identifying_b;
  if (!CopyIdentifyingProperties(properties_b, &identifying_b))
    return false;

  return identifying_a.Equals(&identifying_b);
}

}  // namespace shill_property_util

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
