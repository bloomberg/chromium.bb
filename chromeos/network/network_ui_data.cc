// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_ui_data.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {

// Top-level UI data dictionary keys.
const char NetworkUIData::kKeyONCSource[] = "onc_source";
const char NetworkUIData::kKeyCertificatePattern[] = "certificate_pattern";
const char NetworkUIData::kKeyCertificateType[] = "certificate_type";
const char NetworkUIData::kKeyUserSettings[] = "user_settings";

namespace {

template <typename Enum>
struct StringEnumEntry {
  const char* string;
  Enum enum_value;
};

const StringEnumEntry<onc::ONCSource> kONCSourceTable[] = {
  { "user_import", onc::ONC_SOURCE_USER_IMPORT },
  { "device_policy", onc::ONC_SOURCE_DEVICE_POLICY },
  { "user_policy", onc::ONC_SOURCE_USER_POLICY }
};

const StringEnumEntry<ClientCertType> kClientCertTable[] = {
  { "none", CLIENT_CERT_TYPE_NONE },
  { "pattern", CLIENT_CERT_TYPE_PATTERN },
  { "ref", CLIENT_CERT_TYPE_REF }
};

// Converts |enum_value| to the corresponding string according to |table|. If no
// enum value of the table matches (which can only occur if incorrect casting
// was used to obtain |enum_value|), returns an empty string instead.
template <typename Enum, int N>
std::string EnumToString(const StringEnumEntry<Enum>(& table)[N],
                         Enum enum_value) {
  for (int i = 0; i < N; ++i) {
    if (table[i].enum_value == enum_value)
      return table[i].string;
  }
  return std::string();
}

// Converts |str| to the corresponding enum value according to |table|. If no
// string of the table matches, returns |fallback| instead.
template<typename Enum, int N>
Enum StringToEnum(const StringEnumEntry<Enum>(& table)[N],
                  const std::string& str,
                  Enum fallback) {
  for (int i = 0; i < N; ++i) {
    if (table[i].string == str)
      return table[i].enum_value;
  }
  return fallback;
}

}  // namespace

NetworkUIData::NetworkUIData()
    : onc_source_(onc::ONC_SOURCE_NONE),
      certificate_type_(CLIENT_CERT_TYPE_NONE) {
}

NetworkUIData::NetworkUIData(const NetworkUIData& other) {
  *this = other;
}

NetworkUIData& NetworkUIData::operator=(const NetworkUIData& other) {
  certificate_pattern_ = other.certificate_pattern_;
  onc_source_ = other.onc_source_;
  certificate_type_ = other.certificate_type_;
  if (other.user_settings_)
    user_settings_.reset(other.user_settings_->DeepCopy());
  policy_guid_ = other.policy_guid_;
  return *this;
}

NetworkUIData::NetworkUIData(const DictionaryValue& dict) {
  std::string source;
  dict.GetString(kKeyONCSource, &source);
  onc_source_ = StringToEnum(kONCSourceTable, source, onc::ONC_SOURCE_NONE);

  std::string type_string;
  dict.GetString(kKeyCertificateType, &type_string);
  certificate_type_ =
      StringToEnum(kClientCertTable, type_string, CLIENT_CERT_TYPE_NONE);

  if (certificate_type_ == CLIENT_CERT_TYPE_PATTERN) {
    const DictionaryValue* cert_dict = NULL;
    dict.GetDictionary(kKeyCertificatePattern, &cert_dict);
    if (cert_dict)
      certificate_pattern_.CopyFromDictionary(*cert_dict);
    if (certificate_pattern_.Empty()) {
      LOG(ERROR) << "Couldn't parse a valid certificate pattern.";
      certificate_type_ = CLIENT_CERT_TYPE_NONE;
    }
  }

  const DictionaryValue* user_settings = NULL;
  if (dict.GetDictionary(kKeyUserSettings, &user_settings))
    user_settings_.reset(user_settings->DeepCopy());
}

NetworkUIData::~NetworkUIData() {
}

void NetworkUIData::FillDictionary(base::DictionaryValue* dict) const {
  dict->Clear();

  std::string source_string = EnumToString(kONCSourceTable, onc_source_);
  if (!source_string.empty())
    dict->SetString(kKeyONCSource, source_string);

  if (certificate_type_ != CLIENT_CERT_TYPE_NONE) {
    std::string type_string = EnumToString(kClientCertTable, certificate_type_);
    dict->SetString(kKeyCertificateType, type_string);

    if (certificate_type_ == CLIENT_CERT_TYPE_PATTERN &&
        !certificate_pattern_.Empty()) {
      dict->Set(kKeyCertificatePattern,
                certificate_pattern_.CreateAsDictionary());
    }
  }
  if (user_settings_)
    dict->SetWithoutPathExpansion(kKeyUserSettings,
                                  user_settings_->DeepCopy());
}

namespace {

void TranslateClientCertType(const std::string& client_cert_type,
                             NetworkUIData* ui_data) {
  using namespace onc::certificate;
  ClientCertType type;
  if (client_cert_type == kNone) {
    type = CLIENT_CERT_TYPE_NONE;
  } else if (client_cert_type == kRef) {
    type = CLIENT_CERT_TYPE_REF;
  } else if (client_cert_type == kPattern) {
    type = CLIENT_CERT_TYPE_PATTERN;
  } else {
    type = CLIENT_CERT_TYPE_NONE;
    NOTREACHED();
  }

  ui_data->set_certificate_type(type);
}

void TranslateCertificatePattern(const base::DictionaryValue& onc_object,
                                 NetworkUIData* ui_data) {
  CertificatePattern pattern;
  bool success = pattern.CopyFromDictionary(onc_object);
  DCHECK(success);
  ui_data->set_certificate_pattern(pattern);
}

void TranslateEAP(const base::DictionaryValue& eap,
                  NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (eap.GetStringWithoutPathExpansion(onc::eap::kClientCertType,
                                        &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateIPsec(const base::DictionaryValue& ipsec,
                    NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (ipsec.GetStringWithoutPathExpansion(onc::vpn::kClientCertType,
                                          &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateOpenVPN(const base::DictionaryValue& openvpn,
                      NetworkUIData* ui_data) {
  std::string client_cert_type;
  if (openvpn.GetStringWithoutPathExpansion(onc::vpn::kClientCertType,
                                            &client_cert_type)) {
    TranslateClientCertType(client_cert_type, ui_data);
  }
}

void TranslateONCHierarchy(const onc::OncValueSignature& signature,
                           const base::DictionaryValue& onc_object,
                           NetworkUIData* ui_data) {
  if (&signature == &onc::kCertificatePatternSignature)
    TranslateCertificatePattern(onc_object, ui_data);
  else if (&signature == &onc::kEAPSignature)
    TranslateEAP(onc_object, ui_data);
  else if (&signature == &onc::kIPsecSignature)
    TranslateIPsec(onc_object, ui_data);
  else if (&signature == &onc::kOpenVPNSignature)
    TranslateOpenVPN(onc_object, ui_data);

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(onc_object); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* inner_object;
    if (!it.value().GetAsDictionary(&inner_object))
      continue;

    const onc::OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());

    TranslateONCHierarchy(*field_signature->value_signature, *inner_object,
                          ui_data);
  }
}

}  // namespace

scoped_ptr<NetworkUIData> CreateUIDataFromONC(
    onc::ONCSource onc_source,
    const base::DictionaryValue& onc_network) {
  scoped_ptr<NetworkUIData> ui_data(new NetworkUIData());
  TranslateONCHierarchy(onc::kNetworkConfigurationSignature, onc_network,
                        ui_data.get());

  ui_data->set_onc_source(onc_source);

  return ui_data.Pass();
}

}  // namespace chromeos
