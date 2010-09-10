// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
#pragma once

#include "base/object_watcher.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "base/weak_ptr.h"
#include "chrome/browser/policy/configuration_policy_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

class RegKey;

namespace policy {

// An implementation of |ConfigurationPolicyProvider| using the
// mechanism provided by Windows Groups Policy. Policy decisions are
// stored as values in a special section of the Windows Registry.
// On a managed machine in a domain, this portion of the registry is
// periodically updated by the Windows Group Policy machinery to contain
// the latest version of the policy set by administrators.
class ConfigurationPolicyProviderWin
    : public ConfigurationPolicyProvider,
      public base::SupportsWeakPtr<ConfigurationPolicyProviderWin> {
 public:
  // Keeps watch on Windows Group Policy notification event to trigger a policy
  // reload when Group Policy changes. This class is reference counted to
  // facilitate timer-based reloads through the message loop. It is not safe to
  // access GroupPolicyChangeWatcher concurrently from multiple threads.
  class GroupPolicyChangeWatcher
      : public base::ObjectWatcher::Delegate,
        public MessageLoop::DestructionObserver,
        public base::RefCountedThreadSafe<GroupPolicyChangeWatcher> {
   public:
    GroupPolicyChangeWatcher(
        base::WeakPtr<ConfigurationPolicyProviderWin> provider,
        int reload_interval_minutes);
    virtual ~GroupPolicyChangeWatcher();

    // Start watching.
    void Start();

    // Stop any pending watch activity in order to allow for timely shutdown.
    void Stop();

   private:
    // Updates the watchers and schedules the reload task if appropriate.
    void SetupWatches();

    // Post a reload notification and update the watch machinery.
    void Reload();

    // Called for timer-based refresh from the message loop.
    void ReloadFromTask();

    // ObjectWatcher::Delegate implementation:
    virtual void OnObjectSignaled(HANDLE object);

    // MessageLoop::DestructionObserver implementation:
    virtual void WillDestroyCurrentMessageLoop();

    base::WeakPtr<ConfigurationPolicyProviderWin> provider_;
    base::WaitableEvent user_policy_changed_event_;
    base::WaitableEvent machine_policy_changed_event_;
    base::ObjectWatcher user_policy_watcher_;
    base::ObjectWatcher machine_policy_watcher_;
    bool user_policy_watcher_failed_;
    bool machine_policy_watcher_failed_;

    // Period to schedule the reload task at.
    int reload_interval_minutes_;

    // A reference to a delayed task for timer-based reloading.
    CancelableTask* reload_task_;
  };

  ConfigurationPolicyProviderWin();
  virtual ~ConfigurationPolicyProviderWin();

  // ConfigurationPolicyProvider method overrides:
  virtual bool Provide(ConfigurationPolicyStore* store);

 protected:
  // The sub key path for Chromium's Group Policy information in the
  // Windows registry.
  static const wchar_t kPolicyRegistrySubKey[];

 private:
  scoped_refptr<GroupPolicyChangeWatcher> watcher_;

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

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_WIN_H_
