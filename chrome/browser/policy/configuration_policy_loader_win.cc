// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_loader_win.h"

#include <userenv.h>

#include "content/browser/browser_thread.h"

namespace policy {

ConfigurationPolicyLoaderWin::ConfigurationPolicyLoaderWin(
    AsynchronousPolicyProvider::Delegate* delegate,
    int reload_interval_minutes)
    : AsynchronousPolicyLoader(delegate, reload_interval_minutes),
      user_policy_changed_event_(false, false),
      machine_policy_changed_event_(false, false),
      user_policy_watcher_failed_(false),
      machine_policy_watcher_failed_(false) {
  if (!RegisterGPNotification(user_policy_changed_event_.handle(), false)) {
    PLOG(WARNING) << "Failed to register user group policy notification";
    user_policy_watcher_failed_ = true;
  }
  if (!RegisterGPNotification(machine_policy_changed_event_.handle(), true)) {
    PLOG(WARNING) << "Failed to register machine group policy notification.";
    machine_policy_watcher_failed_ = true;
  }
}

void ConfigurationPolicyLoaderWin::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  AsynchronousPolicyLoader::InitOnFileThread();
  SetupWatches();
}

void ConfigurationPolicyLoaderWin::StopOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  user_policy_watcher_.StopWatching();
  machine_policy_watcher_.StopWatching();
  AsynchronousPolicyLoader::StopOnFileThread();
}

void ConfigurationPolicyLoaderWin::SetupWatches() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  CancelReloadTask();

  if (!user_policy_watcher_failed_ &&
      !user_policy_watcher_.GetWatchedObject() &&
      !user_policy_watcher_.StartWatching(
          user_policy_changed_event_.handle(), this)) {
    LOG(WARNING) << "Failed to start watch for user policy change event";
    user_policy_watcher_failed_ = true;
  }
  if (!machine_policy_watcher_failed_ &&
      !machine_policy_watcher_.GetWatchedObject() &&
      !machine_policy_watcher_.StartWatching(
          machine_policy_changed_event_.handle(), this)) {
    LOG(WARNING) << "Failed to start watch for machine policy change event";
    machine_policy_watcher_failed_ = true;
   }

  if (user_policy_watcher_failed_ || machine_policy_watcher_failed_)
    ScheduleFallbackReloadTask();
}

void ConfigurationPolicyLoaderWin::Reload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  AsynchronousPolicyLoader::Reload();
  SetupWatches();
}

void ConfigurationPolicyLoaderWin::OnObjectSignaled(HANDLE object) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(object == user_policy_changed_event_.handle() ||
         object == machine_policy_changed_event_.handle())
      << "unexpected object signaled policy reload, obj = "
      << std::showbase << std::hex << object;
  Reload();
}

}  // namespace policy
