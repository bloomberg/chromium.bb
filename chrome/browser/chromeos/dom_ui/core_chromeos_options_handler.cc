// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/core_chromeos_options_handler.h"

#include "base/json/json_reader.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros_settings.h"

namespace chromeos {

Value* CoreChromeOSOptionsHandler::FetchPref(const std::wstring& pref_name) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::FetchPref(pref_name);

  Value* pref_value = NULL;
  CrosSettings::Get()->Get(pref_name, &pref_value);
  return pref_value ? pref_value->DeepCopy() : Value::CreateNullValue();
}

void CoreChromeOSOptionsHandler::ObservePref(const std::wstring& pref_name) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::ObservePref(pref_name);

  // TODO(xiyuan): Change this when CrosSettings supports observers.
}

void CoreChromeOSOptionsHandler::SetPref(const std::wstring& pref_name,
                                         Value::ValueType pref_type,
                                         const std::string& value_string) {
  if (!CrosSettings::IsCrosSettings(pref_name))
    return ::CoreOptionsHandler::SetPref(pref_name, pref_type, value_string);

  CrosSettings* cros_settings = CrosSettings::Get();
  switch (pref_type) {
    case Value::TYPE_BOOLEAN:
      cros_settings->SetBoolean(pref_name, value_string == "true");
      break;
    case Value::TYPE_INTEGER:
      int int_value;
      if (StringToInt(value_string, &int_value))
        cros_settings->SetInteger(pref_name, int_value);
      break;
    case Value::TYPE_STRING:
      cros_settings->SetString(pref_name, value_string);
      break;
    default: {
      Value* pref_value = base::JSONReader().JsonToValue(
          value_string,  // JSON
          false,         // no check_root
          false);        // no trailing comma
      if (pref_value)
        cros_settings->Set(pref_name, pref_value);
      break;
    }
  }
}

}  // namespace chromeos
