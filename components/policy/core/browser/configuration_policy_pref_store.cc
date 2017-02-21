// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_pref_store.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/browser/configuration_policy_handler_list.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/prefs/pref_value_map.h"

namespace policy {

namespace {

// Policies are loaded early on startup, before PolicyErrorMaps are ready to
// be retrieved. This function is posted to UI to log any errors found on
// Refresh below.
void LogErrors(PolicyErrorMap* errors) {
  PolicyErrorMap::const_iterator iter;
  for (iter = errors->begin(); iter != errors->end(); ++iter) {
    base::string16 policy = base::ASCIIToUTF16(iter->first);
    DLOG(WARNING) << "Policy " << policy << ": " << iter->second;
  }
}

bool IsLevel(PolicyLevel level, const PolicyMap::const_iterator iter) {
  return iter->second.level == level;
}

}  // namespace

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    PolicyService* service,
    const ConfigurationPolicyHandlerList* handler_list,
    PolicyLevel level)
    : policy_service_(service),
      handler_list_(handler_list),
      level_(level) {
  // Read initial policy.
  prefs_.reset(CreatePreferencesFromPolicies());
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigurationPolicyPrefStore::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ConfigurationPolicyPrefStore::HasObservers() const {
  return observers_.might_have_observers();
}

bool ConfigurationPolicyPrefStore::IsInitializationComplete() const {
  return policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME);
}

bool ConfigurationPolicyPrefStore::GetValue(const std::string& key,
                                            const base::Value** value) const {
  const base::Value* stored_value = NULL;
  if (!prefs_.get() || !prefs_->GetValue(key, &stored_value))
    return false;

  if (value)
    *value = stored_value;
  return true;
}

std::unique_ptr<base::DictionaryValue> ConfigurationPolicyPrefStore::GetValues()
    const {
  if (!prefs_)
    return base::MakeUnique<base::DictionaryValue>();
  return prefs_->AsDictionaryValue();
}

void ConfigurationPolicyPrefStore::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK_EQ(POLICY_DOMAIN_CHROME, ns.domain);
  DCHECK(ns.component_id.empty());
  Refresh();
}

void ConfigurationPolicyPrefStore::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain == POLICY_DOMAIN_CHROME) {
    for (auto& observer : observers_)
      observer.OnInitializationCompleted(true);
  }
}

ConfigurationPolicyPrefStore::~ConfigurationPolicyPrefStore() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void ConfigurationPolicyPrefStore::Refresh() {
  std::unique_ptr<PrefValueMap> new_prefs(CreatePreferencesFromPolicies());
  std::vector<std::string> changed_prefs;
  new_prefs->GetDifferingKeys(prefs_.get(), &changed_prefs);
  prefs_.swap(new_prefs);

  // Send out change notifications.
  for (std::vector<std::string>::const_iterator pref(changed_prefs.begin());
       pref != changed_prefs.end();
       ++pref) {
    for (auto& observer : observers_)
      observer.OnPrefValueChanged(*pref);
  }
}

PrefValueMap* ConfigurationPolicyPrefStore::CreatePreferencesFromPolicies() {
  std::unique_ptr<PrefValueMap> prefs(new PrefValueMap);
  PolicyMap filtered_policies;
  filtered_policies.CopyFrom(policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())));
  filtered_policies.EraseNonmatching(base::Bind(&IsLevel, level_));

  std::unique_ptr<PolicyErrorMap> errors(new PolicyErrorMap);

  handler_list_->ApplyPolicySettings(filtered_policies,
                                     prefs.get(),
                                     errors.get());

  // Retrieve and log the errors once the UI loop is ready. This is only an
  // issue during startup.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&LogErrors, base::Owned(errors.release())));

  return prefs.release();
}

}  // namespace policy
