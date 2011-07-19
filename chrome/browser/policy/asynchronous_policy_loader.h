// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider.h"

namespace policy {

// Used by the implementation of asynchronous policy provider to manage the
// tasks on the file thread that do the heavy lifting of loading policies.
class AsynchronousPolicyLoader
    : public base::RefCountedThreadSafe<AsynchronousPolicyLoader> {
 public:
  explicit AsynchronousPolicyLoader(
      AsynchronousPolicyProvider::Delegate* delegate,
      int reload_interval_minutes);

  // Triggers initial policy load.
  virtual void Init();

  // Reloads policy, sending notification of changes if necessary. Must be
  // called on the file thread.
  virtual void Reload();

  // Stops any pending reload tasks.
  virtual void Stop();

  void AddObserver(ConfigurationPolicyProvider::Observer* observer);
  void RemoveObserver(ConfigurationPolicyProvider::Observer* observer);

  const DictionaryValue* policy() const { return policy_.get(); }

 protected:
  friend class UpdatePolicyTask;

  // AsynchronousPolicyLoader objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<AsynchronousPolicyLoader>;
  virtual ~AsynchronousPolicyLoader();

  // Schedules a call to UpdatePolicy on |origin_loop_|. Takes ownership of
  // |new_policy|.
  void PostUpdatePolicyTask(DictionaryValue* new_policy);

  AsynchronousPolicyProvider::Delegate* delegate() {
    return delegate_.get();
  }

  // Performs start operations that must be performed on the file thread.
  virtual void InitOnFileThread();

  // Performs stop operations that must be performed on the file thread.
  virtual void StopOnFileThread();

  // Schedules a reload task to run when |delay| expires. Must be called on the
  // file thread.
  void ScheduleReloadTask(const base::TimeDelta& delay);

  // Schedules a reload task to run after the number of minutes specified
  // in |reload_interval_minutes_|. Must be called on the file thread.
  void ScheduleFallbackReloadTask();

  void CancelReloadTask();

  // Invoked from the reload task on the file thread.
  void ReloadFromTask();

 private:
  friend class AsynchronousPolicyLoaderTest;

  // Finishes loader initialization after the threading system has been fully
  // intialized.
  void InitAfterFileThreadAvailable();

  // Replaces the existing policy to value map with a new one, sending
  // notification to the observers if there is a policy change. Must be called
  // on |origin_loop_| so that it's safe to call back into the provider, which
  // is not thread-safe. Takes ownership of |new_policy|.
  void UpdatePolicy(DictionaryValue* new_policy);

  // Provides the low-level mechanics for loading policy.
  scoped_ptr<AsynchronousPolicyProvider::Delegate> delegate_;

  // Current policy.
  scoped_ptr<DictionaryValue> policy_;

  // The reload task. Access only on the file thread. Holds a reference to the
  // currently posted task, so we can cancel and repost it if necessary.
  CancelableTask* reload_task_;

  // The interval at which a policy reload will be triggered as a fallback.
  const base::TimeDelta  reload_interval_;

  // The message loop on which this object was constructed. Recorded so that
  // it's possible to call back into the non thread safe provider to fire the
  // notification.
  MessageLoop* origin_loop_;

  // True if Stop has been called.
  bool stopped_;

  ObserverList<ConfigurationPolicyProvider::Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
