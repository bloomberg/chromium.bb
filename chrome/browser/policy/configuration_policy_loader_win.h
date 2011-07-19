// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_LOADER_WIN_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_LOADER_WIN_H_
#pragma once

#include "base/synchronization/waitable_event.h"
#include "base/win/object_watcher.h"
#include "chrome/browser/policy/asynchronous_policy_loader.h"

namespace policy {

// Keeps watch on Windows Group Policy notification event to trigger a policy
// reload when Group Policy changes.
class ConfigurationPolicyLoaderWin
    : public AsynchronousPolicyLoader,
      public base::win::ObjectWatcher::Delegate {
 public:
  ConfigurationPolicyLoaderWin(
      AsynchronousPolicyProvider::Delegate* delegate,
      int reload_interval_minutes);
  virtual ~ConfigurationPolicyLoaderWin() {}

 protected:
  // AsynchronousPolicyLoader overrides:
  virtual void InitOnFileThread();
  virtual void StopOnFileThread();

 private:
  // Updates the watchers and schedules the reload task if appropriate.
  void SetupWatches();

  // Post a reload notification and update the watch machinery.
  void Reload();

  // ObjectWatcher::Delegate overrides:
  virtual void OnObjectSignaled(HANDLE object);

  base::WaitableEvent user_policy_changed_event_;
  base::WaitableEvent machine_policy_changed_event_;
  base::win::ObjectWatcher user_policy_watcher_;
  base::win::ObjectWatcher machine_policy_watcher_;
  bool user_policy_watcher_failed_;
  bool machine_policy_watcher_failed_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyLoaderWin);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_LOADER_WIN_H_
