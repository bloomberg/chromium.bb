// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#pragma once

#include "base/object_watcher.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "chrome/browser/policy/configuration_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

class RegKey;

// An implementation of |ConfigurationPolicyProvider| using the
// mechanism provided by Windows Groups Policy. Policy decisions are
// stored as values in a special section of the Windows Registry.
// On a managed machine in a domain, this portion of the registry is
// periodically updated by the Windows Group Policy machinery to contain
// the latest version of the policy set by administrators.
class ConfigurationPolicyProviderWin : public ConfigurationPolicyProvider {
 public:
  // Keeps watch on Windows Group Policy notification event to trigger
  // a policy reload when Group Policy changes.
  class GroupPolicyChangeWatcher : public base::ObjectWatcher::Delegate {
   public:
    explicit GroupPolicyChangeWatcher(ConfigurationPolicyProvider* provider);
    virtual ~GroupPolicyChangeWatcher() {}

    virtual void OnObjectSignaled(HANDLE object);

   private:
    ConfigurationPolicyProvider* provider_;
    base::WaitableEvent user_policy_changed_event_;
    base::WaitableEvent machine_policy_changed_event_;
    base::ObjectWatcher user_policy_watcher_;
    base::ObjectWatcher machine_policy_watcher_;
  };

  ConfigurationPolicyProviderWin();
  virtual ~ConfigurationPolicyProviderWin() { }

  // ConfigurationPolicyProvider method overrides:
  virtual bool Provide(ConfigurationPolicyStore* store);

 protected:
  // The sub key path for Chromium's Group Policy information in the
  // Windows registry.
  static const wchar_t kPolicyRegistrySubKey[];

 private:
  scoped_ptr<GroupPolicyChangeWatcher> watcher_;

  // Methods to perform type-specific policy lookups in the registry.
  // HKLM is checked first, then HKCU.

  // Reads a string registry value |name| at the specified |key| and puts the
  // resulting string in |result|.
  bool ReadRegistryStringValue(RegKey* key, const string16& name,
                               string16* result);

  bool GetRegistryPolicyString(const string16& name, string16* result);
  // Gets a list value contained under |key| one level below the policy root.
  bool GetRegistryPolicyStringList(const string16& key, ListValue* result);
  bool GetRegistryPolicyBoolean(const string16& value_name, bool* result);
  bool GetRegistryPolicyInteger(const string16& value_name, uint32* result);
};

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_

