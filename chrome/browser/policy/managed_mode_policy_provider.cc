// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/managed_mode_policy_provider.h"

#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"
#include "policy/policy_constants.h"

using content::BrowserThread;

namespace policy {

// static
const char ManagedModePolicyProvider::kPolicies[] = "policies";

// static
ManagedModePolicyProvider* ManagedModePolicyProvider::Create(Profile* profile) {
  JsonPrefStore* pref_store =
      new JsonPrefStore(profile->GetPath().Append(
                            chrome::kManagedModePolicyFilename),
                        BrowserThread::GetMessageLoopProxyForThread(
                            BrowserThread::FILE));
  return new ManagedModePolicyProvider(pref_store);
}

ManagedModePolicyProvider::ManagedModePolicyProvider(
    PersistentPrefStore* pref_store)
    : store_(pref_store) {
  store_->AddObserver(this);
  store_->ReadPrefsAsync(NULL);
}

ManagedModePolicyProvider::~ManagedModePolicyProvider() {
  store_->RemoveObserver(this);
}

const base::Value* ManagedModePolicyProvider::GetPolicy(
    const std::string& key) const {
  base::DictionaryValue* dict = GetCachedPolicy();
  base::Value* value = NULL;
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

void ManagedModePolicyProvider::RefreshPolicies() {
  UpdatePolicyFromCache();
}

bool ManagedModePolicyProvider::IsInitializationComplete() const {
  return store_->IsInitializationComplete();
}

void ManagedModePolicyProvider::OnPrefValueChanged(const std::string& key) {}

void ManagedModePolicyProvider::OnInitializationCompleted(bool success) {
  DCHECK(success);
  UpdatePolicyFromCache();
}

base::DictionaryValue* ManagedModePolicyProvider::GetCachedPolicy() const {
  base::Value* value = NULL;
  base::DictionaryValue* dict = NULL;
  PrefStore::ReadResult result = store_->GetMutableValue(kPolicies, &value);
  switch (result) {
    case PrefStore::READ_NO_VALUE: {
      dict = new base::DictionaryValue;
      store_->SetValue(kPolicies, dict);
      break;
    }
    case PrefStore::READ_OK: {
      bool success = value->GetAsDictionary(&dict);
      DCHECK(success);
      break;
    }
    default:
      NOTREACHED();
  }

  return dict;
}

void ManagedModePolicyProvider::UpdatePolicyFromCache() {
  scoped_ptr<PolicyBundle> policy_bundle(new PolicyBundle);
  PolicyMap* policy_map =
      &policy_bundle->Get(POLICY_DOMAIN_CHROME, std::string());
  policy_map->LoadFrom(GetCachedPolicy(),
                       POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER);
  UpdatePolicy(policy_bundle.Pass());
}

}  // namespace policy
