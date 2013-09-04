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

void CopyStringFromDictionary(const base::DictionaryValue& source,
                              const std::string& key,
                              base::DictionaryValue* dest) {
  std::string string_value;
  if (source.GetStringWithoutPathExpansion(key, &string_value))
    dest->SetStringWithoutPathExpansion(key, string_value);
}

}  // namespace

std::string GetNameFromProperties(const std::string& service_path,
                                  const base::DictionaryValue& properties) {
  std::string name, hex_ssid;
  properties.GetStringWithoutPathExpansion(flimflam::kNameProperty, &name);
  properties.GetStringWithoutPathExpansion(flimflam::kWifiHexSsid, &hex_ssid);

  if (hex_ssid.empty()) {
    if (name.empty())
      return name;
    // Validate name for UTF8.
    std::string valid_ssid = ValidateUTF8(name);
    if (valid_ssid != name) {
      NET_LOG_DEBUG(
          "GetNameFromProperties",
          base::StringPrintf(
              "%s: UTF8: %s", service_path.c_str(), valid_ssid.c_str()));
    }
    return valid_ssid;
  }

  std::string ssid;
  std::vector<uint8> raw_ssid_bytes;
  if (base::HexStringToBytes(hex_ssid, &raw_ssid_bytes)) {
    ssid = std::string(raw_ssid_bytes.begin(), raw_ssid_bytes.end());
    NET_LOG_DEBUG("GetNameFromProperties",
                  base::StringPrintf("%s: %s, SSID: %s",
                                     service_path.c_str(),
                                     hex_ssid.c_str(),
                                     ssid.c_str()));
  } else {
    NET_LOG_ERROR("GetNameFromProperties",
                  base::StringPrintf("%s: Error processing: %s",
                                     service_path.c_str(),
                                     hex_ssid.c_str()));
    return name;
  }

  if (IsStringUTF8(ssid)) {
    if (ssid != name) {
      NET_LOG_DEBUG("GetNameFromProperties",
                    base::StringPrintf(
                        "%s: UTF8: %s", service_path.c_str(), ssid.c_str()));
    }
    return ssid;
  }

  // Detect encoding and convert to UTF-8.
  std::string country_code;
  properties.GetStringWithoutPathExpansion(flimflam::kCountryProperty,
                                           &country_code);
  std::string encoding;
  if (!base::DetectEncoding(ssid, &encoding)) {
    // TODO(stevenjb): This is currently experimental. If we find a case where
    // base::DetectEncoding() fails, we need to figure out whether we can use
    // country_code with ConvertToUtf8(). crbug.com/233267.
    encoding = country_code;
  }
  if (!encoding.empty()) {
    std::string utf8_ssid;
    if (base::ConvertToUtf8AndNormalize(ssid, encoding, &utf8_ssid)) {
      if (utf8_ssid != name) {
        NET_LOG_DEBUG("GetNameFromProperties",
                      base::StringPrintf("%s: Encoding=%s: %s",
                                         service_path.c_str(),
                                         encoding.c_str(),
                                         utf8_ssid.c_str()));
      }
      return utf8_ssid;
    }
  }

  // Unrecognized encoding. Only use raw bytes if name_ is empty.
  NET_LOG_DEBUG("GetNameFromProperties",
                base::StringPrintf("%s: Unrecognized Encoding=%s: %s",
                                   service_path.c_str(),
                                   encoding.c_str(),
                                   ssid.c_str()));
  if (name.empty() && !ssid.empty())
    return ssid;
  return name;
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
  shill_dictionary.GetWithoutPathExpansion(flimflam::kUIDataProperty,
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
  shill_dictionary->SetStringWithoutPathExpansion(flimflam::kUIDataProperty,
                                                  ui_data_blob);
}

bool CopyIdentifyingProperties(const base::DictionaryValue& service_properties,
                               base::DictionaryValue* dest) {
  CopyStringFromDictionary(service_properties, flimflam::kGuidProperty, dest);

  std::string type;
  service_properties.GetStringWithoutPathExpansion(flimflam::kTypeProperty,
                                                   &type);
  dest->SetStringWithoutPathExpansion(flimflam::kTypeProperty, type);
  if (type == flimflam::kTypeWifi) {
    CopyStringFromDictionary(
        service_properties, flimflam::kSecurityProperty, dest);
    CopyStringFromDictionary(service_properties, flimflam::kSSIDProperty, dest);
    CopyStringFromDictionary(service_properties, flimflam::kModeProperty, dest);
  } else if (type == flimflam::kTypeVPN) {
    CopyStringFromDictionary(service_properties, flimflam::kNameProperty, dest);
    // VPN Provider values are read from the "Provider" dictionary, but written
    // with the keys "Provider.Type" and "Provider.Host".
    const base::DictionaryValue* provider_properties;
    if (!service_properties.GetDictionaryWithoutPathExpansion(
             flimflam::kProviderProperty, &provider_properties)) {
      LOG(ERROR) << "Incomplete Shill dictionary missing VPN provider dict.";
      return false;
    }
    std::string vpn_provider_type;
    provider_properties->GetStringWithoutPathExpansion(flimflam::kTypeProperty,
                                                       &vpn_provider_type);
    dest->SetStringWithoutPathExpansion(flimflam::kProviderTypeProperty,
                                        vpn_provider_type);

    std::string vpn_provider_host;
    provider_properties->GetStringWithoutPathExpansion(flimflam::kHostProperty,
                                                       &vpn_provider_host);
    dest->SetStringWithoutPathExpansion(flimflam::kProviderHostProperty,
                                        vpn_provider_host);
  } else if (type == flimflam::kTypeEthernet ||
             type == shill::kTypeEthernetEap) {
    // Ethernet and EthernetEAP don't have any additional identifying
    // properties.
  } else {
    NOTREACHED() << "Unsupported network type " << type;
  }
  return true;
}

}  // namespace shill_property_util

}  // namespace chromeos
