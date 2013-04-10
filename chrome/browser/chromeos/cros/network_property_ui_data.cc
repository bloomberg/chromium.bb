// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_property_ui_data.h"

#include "base/values.h"
#include "chromeos/network/network_ui_data.h"

namespace chromeos {

// Property names for the per-property dictionary.
const char NetworkPropertyUIData::kKeyController[] = "controller";
const char NetworkPropertyUIData::kKeyDefaultValue[] = "default_value";

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
