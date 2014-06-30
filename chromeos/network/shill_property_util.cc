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
      string_value.empty()) {
    return false;
  }
  dest->SetStringWithoutPathExpansion(key, string_value);
  return true;
}

// This is the same normalization that Shill applies to security types for the
// sake of comparing/identifying WiFi networks. See Shill's
// WiFiService::GetSecurityClass.
std::string GetSecurityClass(const std::string& security) {
  if (security == shill::kSecurityRsn || security == shill::kSecurityWpa)
    return shill::kSecurityPsk;
  else
    return security;
}

}  // namespace

void SetSSID(const std::string ssid, base::DictionaryValue* properties) {
  std::string hex_ssid = base::HexEncode(ssid.c_str(), ssid.size());
  properties->SetStringWithoutPathExpansion(shill::kWifiHexSsid, hex_ssid);
}

std::string GetSSIDFromProperties(const base::DictionaryValue& properties,
                                  bool* unknown_encoding) {
  bool verbose_logging = false;
  if (unknown_encoding) {
    *unknown_encoding = false;
    verbose_logging = true;
  }

  // Get name for debugging.
  std::string name;
  properties.GetStringWithoutPathExpansion(shill::kNameProperty, &name);

  std::string hex_ssid;
  properties.GetStringWithoutPathExpansion(shill::kWifiHexSsid, &hex_ssid);

  if (hex_ssid.empty()) {
    if (verbose_logging)
      NET_LOG_DEBUG("GetSSIDFromProperties: No HexSSID set.", name);
    return std::string();
  }

  std::string ssid;
  std::vector<uint8> raw_ssid_bytes;
  if (base::HexStringToBytes(hex_ssid, &raw_ssid_bytes)) {
    ssid = std::string(raw_ssid_bytes.begin(), raw_ssid_bytes.end());
    if (verbose_logging) {
      NET_LOG_DEBUG(base::StringPrintf("GetSSIDFromProperties: %s, SSID: %s",
                                       hex_ssid.c_str(), ssid.c_str()), name);
    }
  } else {
    NET_LOG_ERROR(
        base::StringPrintf("GetSSIDFromProperties: Error processing: %s",
                           hex_ssid.c_str()), name);
    return std::string();
  }

  if (base::IsStringUTF8(ssid))
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
      if (verbose_logging) {
        NET_LOG_DEBUG(
            base::StringPrintf("GetSSIDFromProperties: Encoding=%s: %s",
                               encoding.c_str(), utf8_ssid.c_str()), name);
      }
    }
    return utf8_ssid;
  }

  if (unknown_encoding)
    *unknown_encoding = true;
  if (verbose_logging) {
    NET_LOG_DEBUG(
        base::StringPrintf("GetSSIDFromProperties: Unrecognized Encoding=%s",
                           encoding.c_str()), name);
  }
  return ssid;
}

std::string GetNetworkIdFromProperties(
    const base::DictionaryValue& properties) {
  if (properties.empty())
    return "EmptyProperties";
  std::string result;
  if (properties.GetStringWithoutPathExpansion(shill::kGuidProperty, &result))
    return result;
  if (properties.GetStringWithoutPathExpansion(shill::kSSIDProperty, &result))
    return result;
  if (properties.GetStringWithoutPathExpansion(shill::kNameProperty, &result))
    return result;
  std::string type = "UnknownType";
  properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  return "Unidentified " + type;
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
  if (type.empty()) {
    NET_LOG_ERROR("GetNameFromProperties: No type", service_path);
    return validated_name;
  }
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
                               const bool properties_read_from_shill,
                               base::DictionaryValue* dest) {
  bool success = true;

  // GUID is optional.
  CopyStringFromDictionary(service_properties, shill::kGuidProperty, dest);

  std::string type;
  service_properties.GetStringWithoutPathExpansion(shill::kTypeProperty, &type);
  success &= !type.empty();
  dest->SetStringWithoutPathExpansion(shill::kTypeProperty, type);
  if (type == shill::kTypeWifi) {
    std::string security;
    service_properties.GetStringWithoutPathExpansion(shill::kSecurityProperty,
                                                     &security);
    if (security.empty()) {
      success = false;
    } else {
      dest->SetStringWithoutPathExpansion(shill::kSecurityProperty,
                                          GetSecurityClass(security));
    }
    success &=
        CopyStringFromDictionary(service_properties, shill::kWifiHexSsid, dest);
    success &= CopyStringFromDictionary(
        service_properties, shill::kModeProperty, dest);
  } else if (type == shill::kTypeVPN) {
    success &= CopyStringFromDictionary(
        service_properties, shill::kNameProperty, dest);

    // VPN Provider values are read from the "Provider" dictionary, but written
    // with the keys "Provider.Type" and "Provider.Host".
    // TODO(pneubeck): Simplify this once http://crbug.com/381135 is fixed.
    std::string vpn_provider_type;
    std::string vpn_provider_host;
    if (properties_read_from_shill) {
      const base::DictionaryValue* provider_properties = NULL;
      if (!service_properties.GetDictionaryWithoutPathExpansion(
              shill::kProviderProperty, &provider_properties)) {
        NET_LOG_ERROR("Missing VPN provider dict",
                      GetNetworkIdFromProperties(service_properties));
      }
      provider_properties->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                         &vpn_provider_type);
      provider_properties->GetStringWithoutPathExpansion(shill::kHostProperty,
                                                         &vpn_provider_host);
    } else {
      service_properties.GetStringWithoutPathExpansion(
          shill::kProviderTypeProperty, &vpn_provider_type);
      service_properties.GetStringWithoutPathExpansion(
          shill::kProviderHostProperty, &vpn_provider_host);
    }
    success &= !vpn_provider_type.empty();
    dest->SetStringWithoutPathExpansion(shill::kProviderTypeProperty,
                                        vpn_provider_type);

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
  if (!success) {
    NET_LOG_ERROR("Missing required properties",
                  GetNetworkIdFromProperties(service_properties));
  }
  return success;
}

bool DoIdentifyingPropertiesMatch(const base::DictionaryValue& new_properties,
                                  const base::DictionaryValue& old_properties) {
  base::DictionaryValue new_identifying;
  if (!CopyIdentifyingProperties(
          new_properties,
          false /* properties were not read from Shill */,
          &new_identifying)) {
    return false;
  }
  base::DictionaryValue old_identifying;
  if (!CopyIdentifyingProperties(old_properties,
                                 true /* properties were read from Shill */,
                                 &old_identifying)) {
    return false;
  }

  return new_identifying.Equals(&old_identifying);
}

bool IsPassphraseKey(const std::string& key) {
  return key == shill::kEapPrivateKeyPasswordProperty ||
      key == shill::kEapPasswordProperty ||
      key == shill::kL2tpIpsecPasswordProperty ||
      key == shill::kOpenVPNPasswordProperty ||
      key == shill::kOpenVPNAuthUserPassProperty ||
      key == shill::kOpenVPNTLSAuthContentsProperty ||
      key == shill::kPassphraseProperty ||
      key == shill::kOpenVPNOTPProperty ||
      key == shill::kEapPrivateKeyProperty ||
      key == shill::kEapPinProperty ||
      key == shill::kApnPasswordProperty;
}

bool GetHomeProviderFromProperty(const base::Value& value,
                                 std::string* home_provider_id) {
  const base::DictionaryValue* dict = NULL;
  if (!value.GetAsDictionary(&dict))
    return false;
  std::string home_provider_country;
  std::string home_provider_name;
  dict->GetStringWithoutPathExpansion(shill::kOperatorCountryKey,
                                      &home_provider_country);
  dict->GetStringWithoutPathExpansion(shill::kOperatorNameKey,
                                      &home_provider_name);
  // Set home_provider_id
  if (!home_provider_name.empty() && !home_provider_country.empty()) {
    *home_provider_id = base::StringPrintf(
        "%s (%s)", home_provider_name.c_str(), home_provider_country.c_str());
  } else {
    if (!dict->GetStringWithoutPathExpansion(shill::kOperatorCodeKey,
                                             home_provider_id)) {
      return false;
    }
    LOG(WARNING)
        << "Provider name and country not defined, using code instead: "
        << *home_provider_id;
  }
  return true;
}

}  // namespace shill_property_util

}  // namespace chromeos
