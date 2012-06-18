// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/persistent_pref_store.h"

class Profile;

namespace policy {

// This class sets policies that are defined by a local user via "Managed Mode".
// It offers methods to set and read policies, and persists them on disk in
// JSON format.
class ManagedModePolicyProvider
    : public ProfileKeyedService,
      public ConfigurationPolicyProvider,
      public PrefStore::Observer {
 public:
  // The dictionary key under which we store the policy dictionary. Public for
  // testing.
  static const char kPolicies[];

  // Creates a new ManagedModePolicyProvider that caches its policies in a JSON
  // file inside the profile folder.
  static ManagedModePolicyProvider* Create(Profile* profile);

  // Use this constructor to inject a different PrefStore (e.g. for testing).
  explicit ManagedModePolicyProvider(PersistentPrefStore* store);
  virtual ~ManagedModePolicyProvider();

  // Returns the stored value for a policy with the given |key|, or NULL if no
  // such policy is defined.
  const base::Value* GetPolicy(const std::string& key) const;
  // Sets the policy with the given |key| to a copy of the given |value|.
  void SetPolicy(const std::string& key, const base::Value* value);

  // ConfigurationPolicyProvider implementation:
  virtual void RefreshPolicies() OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

  // PrefStore::Observer implementation:
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool success) OVERRIDE;

 private:
  base::DictionaryValue* GetCachedPolicy() const;
  void UpdatePolicyFromCache();

  // Used for persisting policies. Unlike other PrefStores, this one is not
  // hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_
