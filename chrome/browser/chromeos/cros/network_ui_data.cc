// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_ui_data.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Top-level UI data dictionary keys.
const char NetworkUIData::kKeyONCSource[] = "onc_source";

// Property names for per-property data stored under |kKeyProperties|.
const EnumMapper<NetworkUIData::ONCSource>::Pair
    NetworkUIData::kONCSourceTable[] =  {
  { "user_import", NetworkUIData::ONC_SOURCE_USER_IMPORT },
  { "device_policy", NetworkUIData::ONC_SOURCE_DEVICE_POLICY },
  { "user_policy", NetworkUIData::ONC_SOURCE_USER_POLICY },
};

// Property names for the per-property dictionary.
const char NetworkPropertyUIData::kKeyController[] = "controller";
const char NetworkPropertyUIData::kKeyDefaultValue[] = "default_value";

NetworkUIData::NetworkUIData()
  : onc_source_(ONC_SOURCE_NONE) {
}

NetworkUIData::~NetworkUIData() {
}

void NetworkUIData::FillDictionary(base::DictionaryValue* dict) const {
  dict->Clear();

  std::string source_string(GetONCSourceMapper().GetKey(onc_source_));
  if (!source_string.empty())
    dict->SetString(kKeyONCSource, source_string);
}

// static
NetworkUIData::ONCSource NetworkUIData::GetONCSource(
    const base::DictionaryValue* ui_data) {
  std::string source;
  if (ui_data && ui_data->GetString(kKeyONCSource, &source))
    return GetONCSourceMapper().Get(source);
  return ONC_SOURCE_NONE;
}

// static
bool NetworkUIData::IsManaged(const base::DictionaryValue* ui_data) {
  ONCSource source = GetONCSource(ui_data);
  return source == ONC_SOURCE_DEVICE_POLICY || source == ONC_SOURCE_USER_POLICY;
}

// static
EnumMapper<NetworkUIData::ONCSource>& NetworkUIData::GetONCSourceMapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ONCSource>, mapper,
                         (kONCSourceTable, arraysize(kONCSourceTable),
                          ONC_SOURCE_NONE));
  return mapper;
}

NetworkPropertyUIData::NetworkPropertyUIData()
    : controller_(CONTROLLER_USER) {
}

NetworkPropertyUIData::~NetworkPropertyUIData() {
}

NetworkPropertyUIData::NetworkPropertyUIData(Controller controller,
                                             base::Value* default_value)
    : controller_(controller),
      default_value_(default_value) {
}

NetworkPropertyUIData::NetworkPropertyUIData(
    const base::DictionaryValue* ui_data) {
  Reset(ui_data);
}

void NetworkPropertyUIData::Reset(const base::DictionaryValue* ui_data) {
  default_value_.reset();
  controller_ = NetworkUIData::IsManaged(ui_data) ? CONTROLLER_POLICY
                                                  : CONTROLLER_USER;
}

void NetworkPropertyUIData::ParseOncProperty(
    const base::DictionaryValue* ui_data,
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

  base::ListValue* recommended_keys = NULL;
  if (onc->GetList(recommended_property_key, &recommended_keys)) {
    base::StringValue basename_value(property_basename);
    if (recommended_keys->Find(basename_value) != recommended_keys->end()) {
      controller_ = CONTROLLER_USER;
      base::Value* default_value = NULL;
      if (onc->Get(property_key, &default_value))
        default_value_.reset(default_value->DeepCopy());
    }
  }
}

}  // namespace chromeos
