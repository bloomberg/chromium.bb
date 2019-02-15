// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/parent_access_code/policy_config_source.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace parent_access {

namespace {

// Dictionary keys for ParentAccessCodeConfig policy.
constexpr char kFutureConfigDictKey[] = "future_config";
constexpr char kCurrentConfigDictKey[] = "current_config";
constexpr char kOldConfigsDictKey[] = "old_configs";
}  // namespace

PolicyConfigSource::PolicyConfigSource(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kParentAccessCodeConfig,
      base::BindRepeating(&PolicyConfigSource::OnPolicyChanged,
                          base::Unretained(this)));
}

PolicyConfigSource::~PolicyConfigSource() = default;

ConfigSource::ConfigSet PolicyConfigSource::GetConfigSet() {
  const base::DictionaryValue* dictionary =
      pref_service_->GetDictionary(prefs::kParentAccessCodeConfig);

  ConfigSet config_set;

  const base::Value* future_config_value = dictionary->FindKeyOfType(
      kFutureConfigDictKey, base::Value::Type::DICTIONARY);
  if (future_config_value) {
    config_set.future_config = AccessCodeConfig::FromDictionary(
        static_cast<const base::DictionaryValue&>(*future_config_value));
  } else {
    LOG(WARNING) << "No future config for parent access code in the policy";
  }

  const base::Value* current_config_value = dictionary->FindKeyOfType(
      kCurrentConfigDictKey, base::Value::Type::DICTIONARY);
  if (future_config_value) {
    config_set.current_config = AccessCodeConfig::FromDictionary(
        static_cast<const base::DictionaryValue&>(*current_config_value));
  } else {
    LOG(WARNING) << "No current config for parent access code in the policy";
  }

  const base::Value* old_configs_value =
      dictionary->FindKeyOfType(kOldConfigsDictKey, base::Value::Type::LIST);
  if (old_configs_value) {
    for (const auto& config : old_configs_value->GetList()) {
      config_set.current_config = AccessCodeConfig::FromDictionary(
          static_cast<const base::DictionaryValue&>(config));
    }
  }
  return config_set;
}

void PolicyConfigSource::OnPolicyChanged() {
  NotifyConfigChanged(GetConfigSet());
}

}  // namespace parent_access
}  // namespace chromeos
