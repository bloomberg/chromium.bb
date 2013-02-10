// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_mode_policy_provider.h"

#include "base/prefs/json_pref_store.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"

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

const base::Value* ManagedModePolicyProvider::GetPolicy(
    const std::string& key) const {
  base::DictionaryValue* dict = GetCachedPolicy();
  base::Value* value = NULL;

  // Returns NULL if the key doesn't exist.
  dict->GetWithoutPathExpansion(key, &value);
  return value;
}

void ManagedModePolicyProvider::SetPolicy(const std::string& key,
                                          const base::Value* value) {
  base::DictionaryValue* dict = GetCachedPolicy();
  dict->SetWithoutPathExpansion(key, value->DeepCopy());
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

base::DictionaryValue* ManagedModePolicyProvider::GetCachedPolicy() const {
  base::Value* value = NULL;
  base::DictionaryValue* dict = NULL;
  if (store_->GetMutableValue(kPolicies, &value)) {
    bool success = value->GetAsDictionary(&dict);
    DCHECK(success);
  } else {
    dict = new base::DictionaryValue;
    store_->SetValue(kPolicies, dict);
  }

  return dict;
}

void ManagedModePolicyProvider::UpdatePolicyFromCache() {
  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  PolicyMap* policy_map =
      &policy_bundle->Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  policy_map->LoadFrom(GetCachedPolicy(),
                       POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  UpdatePolicy(policy_bundle.Pass());
}

}  // namespace policy
