// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_mode_policy_provider.h"

#include "base/prefs/json_pref_store.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/managed_mode/managed_mode_url_filter.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "policy/policy_constants.h"

using base::DictionaryValue;
using base::Value;
using content::BrowserThread;

namespace policy {

// static
const char ManagedModePolicyProvider::kPolicies[] = "policies";

// static
scoped_ptr<ManagedModePolicyProvider> ManagedModePolicyProvider::Create(
    Profile* profile,
    base::SequencedTaskRunner* sequenced_task_runner,
    bool force_load) {
  base::FilePath path =
      profile->GetPath().Append(chrome::kManagedModePolicyFilename);
  JsonPrefStore* pref_store = new JsonPrefStore(path, sequenced_task_runner);
  // Load the data synchronously if needed (when creating profiles on startup).
  if (force_load)
    pref_store->ReadPrefs();
  else
    pref_store->ReadPrefsAsync(NULL);

  return make_scoped_ptr(new ManagedModePolicyProvider(pref_store));
}

ManagedModePolicyProvider::ManagedModePolicyProvider(
    PersistentPrefStore* pref_store)
    : store_(pref_store) {
  store_->AddObserver(this);
  if (store_->IsInitializationComplete())
    UpdatePolicyFromCache();
}

ManagedModePolicyProvider::~ManagedModePolicyProvider() {}

void ManagedModePolicyProvider::InitDefaults() {
  DCHECK(store_->IsInitializationComplete());
  if (GetCachedPolicy())
    return;

  DictionaryValue* dict = new DictionaryValue;
  dict->SetInteger(policy::key::kContentPackDefaultFilteringBehavior,
                   ManagedModeURLFilter::ALLOW);
  dict->SetBoolean(policy::key::kForceSafeSearch, true);

  store_->SetValue(kPolicies, dict);
  UpdatePolicyFromCache();
}

void ManagedModePolicyProvider::InitDefaultsForTesting(
    scoped_ptr<DictionaryValue> dict) {
  DCHECK(store_->IsInitializationComplete());
  DCHECK(!GetCachedPolicy());
  store_->SetValue(kPolicies, dict.release());
  UpdatePolicyFromCache();
}

const DictionaryValue* ManagedModePolicyProvider::GetPolicies() const {
  DictionaryValue* dict = GetCachedPolicy();
  DCHECK(dict);
  return dict;
}

const Value* ManagedModePolicyProvider::GetPolicy(
    const std::string& key) const {
  DictionaryValue* dict = GetCachedPolicy();
  Value* value = NULL;

  // Returns NULL if the key doesn't exist.
  dict->GetWithoutPathExpansion(key, &value);
  return value;
}

scoped_ptr<DictionaryValue> ManagedModePolicyProvider::GetPolicyDictionary(
    const std::string& key) {
  const Value* value = GetPolicy(key);
  const DictionaryValue* dict = NULL;
  if (value && value->GetAsDictionary(&dict))
    return make_scoped_ptr(dict->DeepCopy());

  return make_scoped_ptr(new DictionaryValue);
}

void ManagedModePolicyProvider::SetPolicy(const std::string& key,
                                          scoped_ptr<Value> value) {
  DictionaryValue* dict = GetCachedPolicy();
  if (value)
    dict->SetWithoutPathExpansion(key, value.release());
  else
    dict->RemoveWithoutPathExpansion(key, NULL);

  store_->ReportValueChanged(kPolicies);
  UpdatePolicyFromCache();
}

void ManagedModePolicyProvider::Shutdown() {
  store_->RemoveObserver(this);
  ConfigurationPolicyProvider::Shutdown();
}

void ManagedModePolicyProvider::RefreshPolicies() {
  UpdatePolicyFromCache();
}

bool ManagedModePolicyProvider::IsInitializationComplete(
    PolicyDomain domain) const {
  if (domain == POLICY_DOMAIN_CHROME)
    return store_->IsInitializationComplete();
  return true;
}

void ManagedModePolicyProvider::OnPrefValueChanged(const std::string& key) {}

void ManagedModePolicyProvider::OnInitializationCompleted(bool success) {
  DCHECK(success);
  UpdatePolicyFromCache();
}

DictionaryValue* ManagedModePolicyProvider::GetCachedPolicy() const {
  Value* value = NULL;
  if (!store_->GetMutableValue(kPolicies, &value))
    return NULL;

  DictionaryValue* dict = NULL;
  bool success = value->GetAsDictionary(&dict);
  DCHECK(success);
  return dict;
}

void ManagedModePolicyProvider::UpdatePolicyFromCache() {
  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  DictionaryValue* policies = GetCachedPolicy();
  if (policies) {
    PolicyMap* policy_map =
        &policy_bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME,
                                            std::string()));
    policy_map->LoadFrom(policies, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  }
  UpdatePolicy(policy_bundle.Pass());
}

}  // namespace policy
