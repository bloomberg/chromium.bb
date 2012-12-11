// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_ui_data.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Top-level UI data dictionary keys.
const char NetworkUIData::kKeyONCSource[] = "onc_source";
const char NetworkUIData::kKeyCertificatePattern[] = "certificate_pattern";
const char NetworkUIData::kKeyCertificateType[] = "certificate_type";

// Property names for per-property data stored under |kKeyProperties|.
const EnumMapper<onc::ONCSource>::Pair
    NetworkUIData::kONCSourceTable[] =  {
  { "user_import", onc::ONC_SOURCE_USER_IMPORT },
  { "device_policy", onc::ONC_SOURCE_DEVICE_POLICY },
  { "user_policy", onc::ONC_SOURCE_USER_POLICY },
};

// Property names for per-property data stored under |kKeyProperties|.
const EnumMapper<ClientCertType>::Pair
    NetworkUIData::kClientCertTable[] =  {
  { "none", CLIENT_CERT_TYPE_NONE },
  { "pattern", CLIENT_CERT_TYPE_PATTERN },
  { "ref", CLIENT_CERT_TYPE_REF },
};

// Property names for the per-property dictionary.
const char NetworkPropertyUIData::kKeyController[] = "controller";
const char NetworkPropertyUIData::kKeyDefaultValue[] = "default_value";

NetworkUIData::NetworkUIData()
    : onc_source_(onc::ONC_SOURCE_NONE),
      certificate_type_(CLIENT_CERT_TYPE_NONE) {
}

NetworkUIData::NetworkUIData(const DictionaryValue& dict) {
  std::string source;
  if (dict.GetString(kKeyONCSource, &source)) {
    onc_source_ = GetONCSourceMapper().Get(source);
  } else {
    onc_source_ = onc::ONC_SOURCE_NONE;
  }
  const DictionaryValue* cert_dict = NULL;
  if (dict.GetDictionary(kKeyCertificatePattern, &cert_dict) && cert_dict)
    certificate_pattern_.CopyFromDictionary(*cert_dict);
  std::string type_string;
  if (dict.GetString(kKeyCertificateType, &type_string)) {
    certificate_type_ = GetClientCertMapper().Get(type_string);
  } else {
    certificate_type_ = CLIENT_CERT_TYPE_NONE;
  }
  DCHECK(certificate_type_ != CLIENT_CERT_TYPE_PATTERN ||
         (certificate_type_ == CLIENT_CERT_TYPE_PATTERN &&
          !certificate_pattern_.Empty()));
}

NetworkUIData::~NetworkUIData() {
}

void NetworkUIData::FillDictionary(base::DictionaryValue* dict) const {
  dict->Clear();

  std::string source_string(GetONCSourceMapper().GetKey(onc_source_));
  if (!source_string.empty())
    dict->SetString(kKeyONCSource, source_string);
  std::string type_string(GetClientCertMapper().GetKey(certificate_type_));
  switch (certificate_type_) {
    case CLIENT_CERT_TYPE_REF:
      dict->SetString(kKeyCertificateType, "ref");
      break;
    case CLIENT_CERT_TYPE_PATTERN:
      dict->SetString(kKeyCertificateType, "pattern");
      if (!certificate_pattern_.Empty()) {
        dict->Set(kKeyCertificatePattern,
                  certificate_pattern_.CreateAsDictionary());
      }
    case CLIENT_CERT_TYPE_NONE:
    default:
      break;
  }
}

// static
EnumMapper<onc::ONCSource>& NetworkUIData::GetONCSourceMapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<onc::ONCSource>, mapper,
                         (kONCSourceTable, arraysize(kONCSourceTable),
                          onc::ONC_SOURCE_NONE));
  return mapper;
}

// static
EnumMapper<ClientCertType>& NetworkUIData::GetClientCertMapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ClientCertType>, mapper,
                         (kClientCertTable, arraysize(kClientCertTable),
                          CLIENT_CERT_TYPE_NONE));
  return mapper;
}

NetworkPropertyUIData::NetworkPropertyUIData()
    : controller_(CONTROLLER_USER) {
}

NetworkPropertyUIData::~NetworkPropertyUIData() {
}

NetworkPropertyUIData::NetworkPropertyUIData(
    const NetworkUIData& ui_data) {
  Reset(ui_data);
}

void NetworkPropertyUIData::Reset(const NetworkUIData& ui_data) {
  default_value_.reset();
  controller_ = ui_data.is_managed() ? CONTROLLER_POLICY : CONTROLLER_USER;
}

void NetworkPropertyUIData::ParseOncProperty(
    const NetworkUIData& ui_data,
    const base::DictionaryValue* onc,
    const std::string& property_key) {
  Reset(ui_data);
  if (!onc || controller_ == CONTROLLER_USER)
    return;

  size_t pos = property_key.find_last_of('.');
  std::string recommended_property_key;
  std::string property_basename(property_key);
  if (pos != std::string::npos) {
    recommended_property_key = property_key.substr(0, pos + 1);
    property_basename = property_key.substr(pos + 1);
  }
  recommended_property_key += "Recommended";

  const base::ListValue* recommended_keys = NULL;
  if (onc->GetList(recommended_property_key, &recommended_keys)) {
    base::StringValue basename_value(property_basename);
    if (recommended_keys->Find(basename_value) != recommended_keys->end()) {
      controller_ = CONTROLLER_USER;
      const base::Value* default_value = NULL;
      if (onc->Get(property_key, &default_value))
        default_value_.reset(default_value->DeepCopy());
    }
  }
}

}  // namespace chromeos
