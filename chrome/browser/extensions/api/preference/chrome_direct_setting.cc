// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/preference/chrome_direct_setting.h"

#include "base/containers/hash_tables.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/preference/preference_api_constants.h"
#include "chrome/browser/profiles/profile.h"

namespace extensions {
namespace chromedirectsetting {

namespace {

class PreferenceWhitelist {
 public:
  PreferenceWhitelist() {
    whitelist_.insert("googlegeolocationaccess.enabled");
  }

  ~PreferenceWhitelist() {}

  bool IsPreferenceOnWhitelist(const std::string& pref_key){
    return whitelist_.find(pref_key) != whitelist_.end();
  }

 private:
  base::hash_set<std::string> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(PreferenceWhitelist);
};

static base::LazyInstance<PreferenceWhitelist> preference_whitelist_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DirectSettingFunctionBase::DirectSettingFunctionBase() {}

DirectSettingFunctionBase::~DirectSettingFunctionBase() {}

PrefService* DirectSettingFunctionBase::GetPrefService() {
  return profile()->GetPrefs();
}

bool DirectSettingFunctionBase::IsCalledFromComponentExtension() {
  return GetExtension()->location() == Manifest::COMPONENT;
}

bool DirectSettingFunctionBase::IsPreferenceOnWhitelist(
    const std::string& pref_key) {
  return preference_whitelist_.Get().IsPreferenceOnWhitelist(pref_key);
}

GetDirectSettingFunction::GetDirectSettingFunction() {}

bool GetDirectSettingFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(IsCalledFromComponentExtension());

  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(IsPreferenceOnWhitelist(pref_key));

  const PrefService::Preference* preference =
      GetPrefService()->FindPreference(pref_key.c_str());
  EXTENSION_FUNCTION_VALIDATE(preference);
  const base::Value* value = preference->GetValue();

  scoped_ptr<DictionaryValue> result(new DictionaryValue);
  result->Set(preference_api_constants::kValue, value->DeepCopy());
  SetResult(result.release());

  return true;
}

GetDirectSettingFunction::~GetDirectSettingFunction() {}

SetDirectSettingFunction::SetDirectSettingFunction() {}

bool SetDirectSettingFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(IsCalledFromComponentExtension());

  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(IsPreferenceOnWhitelist(pref_key));

  DictionaryValue* details = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &details));

  Value* value = NULL;
  EXTENSION_FUNCTION_VALIDATE(
      details->Get(preference_api_constants::kValue, &value));

  PrefService* pref_service = GetPrefService();
  const PrefService::Preference* preference =
      pref_service->FindPreference(pref_key.c_str());
  EXTENSION_FUNCTION_VALIDATE(preference);

  EXTENSION_FUNCTION_VALIDATE(value->GetType() == preference->GetType());

  pref_service->Set(pref_key.c_str(), *value);

  return true;
}

SetDirectSettingFunction::~SetDirectSettingFunction() {}

ClearDirectSettingFunction::ClearDirectSettingFunction() {}

bool ClearDirectSettingFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(IsCalledFromComponentExtension());

  std::string pref_key;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &pref_key));
  EXTENSION_FUNCTION_VALIDATE(IsPreferenceOnWhitelist(pref_key));
  GetPrefService()->ClearPref(pref_key.c_str());

  return true;
}

ClearDirectSettingFunction::~ClearDirectSettingFunction() {}

}  // namespace chromedirectsetting
}  // namespace extensions

