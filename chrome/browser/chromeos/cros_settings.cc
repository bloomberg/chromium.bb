// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_settings.h"

#include "base/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings_provider.h"
#include "chrome/browser/chromeos/cros_settings_provider_user.h"

namespace chromeos {

CrosSettings* CrosSettings::Get() {
  // TODO(xiyaun): Use real stuff when underlying libcros is ready.
  return Singleton<CrosSettings>::get();
}

bool CrosSettings::IsCrosSettings(const std::string& path) {
  return StartsWithASCII(path, kCrosSettingsPrefix, true);
}

void CrosSettings::SetBoolean(const std::string& path, bool in_value) {
  Set(path, Value::CreateBooleanValue(in_value));
}

void CrosSettings::SetInteger(const std::string& path, int in_value) {
  Set(path, Value::CreateIntegerValue(in_value));
}

void CrosSettings::SetReal(const std::string& path, double in_value) {
  Set(path, Value::CreateRealValue(in_value));
}

void CrosSettings::SetString(const std::string& path,
                             const std::string& in_value) {
  Set(path, Value::CreateStringValue(in_value));
}

bool CrosSettings::GetBoolean(const std::string& path,
                              bool* bool_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsBoolean(bool_value);
}

bool CrosSettings::AddProvider(CrosSettingsProvider* provider) {
  providers_.push_back(provider);
  return true;
}

bool CrosSettings::RemoveProvider(CrosSettingsProvider* provider) {
  std::vector<CrosSettingsProvider*>::iterator it =
      std::find(providers_.begin(), providers_.end(), provider);
  if (it != providers_.end()) {
    providers_.erase(it);
    return true;
  }
  return false;
}

CrosSettingsProvider* CrosSettings::GetProvider(
    const std::string& path) const {
  for (size_t i = 0; i < providers_.size(); ++i) {
    if (providers_[i]->HandlesSetting(path)){
      return providers_[i];
    }
  }
  return NULL;
}

void CrosSettings::Set(const std::string& path, Value* in_value) {
  CrosSettingsProvider* provider;
  provider = GetProvider(path);
  if (provider) {
    provider->Set(path, in_value);
  }
}

bool CrosSettings::Get(const std::string& path, Value** out_value) const {
  CrosSettingsProvider* provider;
  provider = GetProvider(path);
  if (provider) {
    return provider->Get(path, out_value);
  }
  return false;
}

bool CrosSettings::GetInteger(const std::string& path,
                              int* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsInteger(out_value);
}

bool CrosSettings::GetReal(const std::string& path,
                           double* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsReal(out_value);
}

bool CrosSettings::GetString(const std::string& path,
                             std::string* out_value) const {
  Value* value;
  if (!Get(path, &value))
    return false;

  return value->GetAsString(out_value);
}

CrosSettings::CrosSettings() {
  UserCrosSettingsProvider* user = new UserCrosSettingsProvider();
  AddProvider(user);
}

CrosSettings::~CrosSettings() {
  for (size_t i = 0; i < providers_.size(); ++i) {
    delete providers_[i];
  }
}

}  // namespace chromeos
