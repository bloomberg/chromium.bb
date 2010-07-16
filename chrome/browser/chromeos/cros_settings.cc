// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings.h"

#include "base/singleton.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/mock_cros_settings.h"

namespace chromeos {

CrosSettings* CrosSettings::Get() {
  // TODO(xiyaun): Use real stuff when underlying libcros is ready.
  return Singleton<MockCrosSettings>::get();
}

bool CrosSettings::IsCrosSettings(const std::wstring& path) {
  return StartsWith(path, kCrosSettingsPrefix, true);
}

void CrosSettings::SetBoolean(const std::wstring& path, bool in_value) {
  Set(path, Value::CreateBooleanValue(in_value));
}

void CrosSettings::SetInteger(const std::wstring& path, int in_value) {
  Set(path, Value::CreateIntegerValue(in_value));
}

void CrosSettings::SetReal(const std::wstring& path, double in_value) {
  Set(path, Value::CreateRealValue(in_value));
}

void CrosSettings::SetString(const std::wstring& path,
                             const std::string& in_value) {
  Set(path, Value::CreateStringValue(in_value));
}

bool CrosSettings::GetBoolean(const std::wstring& path,
                              bool* bool_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool CrosSettings::GetInteger(const std::wstring& path,
                              int* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool CrosSettings::GetReal(const std::wstring& path,
                           double* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsReal(out_value);
}

bool CrosSettings::GetString(const std::wstring& path,
                             std::string* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

}  // namespace chromeos
