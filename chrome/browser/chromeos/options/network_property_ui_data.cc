// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/network_property_ui_data.h"

#include "base/values.h"

namespace chromeos {

NetworkPropertyUIData::NetworkPropertyUIData()
    : onc_source_(onc::ONC_SOURCE_NONE) {
}

NetworkPropertyUIData::NetworkPropertyUIData(onc::ONCSource onc_source)
    : onc_source_(onc_source) {
}

NetworkPropertyUIData::~NetworkPropertyUIData() {
}

void NetworkPropertyUIData::ParseOncProperty(onc::ONCSource onc_source,
                                             const base::DictionaryValue* onc,
                                             const std::string& property_key) {
  default_value_.reset();
  onc_source_ = onc_source;

  if (!onc || !IsManaged())
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
      onc_source_ = onc::ONC_SOURCE_NONE;
      const base::Value* default_value = NULL;
      if (onc->Get(property_key, &default_value))
        default_value_.reset(default_value->DeepCopy());
    }
  }
}

}  // namespace chromeos
