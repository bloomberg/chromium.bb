// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
#pragma once

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/policy/asynchronous_policy_provider.h"

class MessageLoop;

namespace policy {

class PolicyBundle;

// Used by the implementation of asynchronous policy provider to manage the
// tasks on the FILE thread that do the heavy lifting of loading policies.
class AsynchronousPolicyLoader
    : public base::RefCountedThreadSafe<AsynchronousPolicyLoader> {
 public:
  // The type of the callback passed to Init().
  typedef base::Callback<void(scoped_ptr<PolicyBundle>)> UpdateCallback;

  AsynchronousPolicyLoader(AsynchronousPolicyProvider::Delegate* delegate,
                           int reload_interval_minutes);

  // Triggers initial policy load, and installs |callback| as the callback to
  // invoke on policy updates. |callback| takes ownership of the passed
  // PolicyBundle, which contains all the policies that were loaded.
  virtual void Init(const UpdateCallback& callback);

  // Reloads policy, sending notification of changes if necessary. Must be
  // called on the FILE thread. When |force| is true, the loader should do an
  // immediate full reload.
  virtual void Reload(bool force);

  // Stops any pending reload tasks. Updates callbacks won't be performed
  // anymore once the loader is stopped.
  virtual void Stop();

 protected:
  // AsynchronousPolicyLoader objects should only be deleted by
  // RefCountedThreadSafe.
  friend class base::RefCountedThreadSafe<AsynchronousPolicyLoader>;
  virtual ~AsynchronousPolicyLoader();

  // Schedules a call to UpdatePolicy on |origin_loop_|.
  void PostUpdatePolicyTask(scoped_ptr<PolicyBundle> bundle);

  AsynchronousPolicyProvider::Delegate* delegate() {
    return delegate_.get();
  }

  // Performs start operations that must be performed on the FILE thread.
  virtual void InitOnFileThread();

  // Performs stop operations that must be performed on the FILE thread.
  virtual void StopOnFileThread();

  // Schedules a reload task to run when |delay| expires. Must be called on the
  // FILE thread.
  void ScheduleReloadTask(const base::TimeDelta& delay);

  // Schedules a reload task to run after the number of minutes specified
  // in |reload_interval_minutes_|. Must be called on the FILE thread.
  void ScheduleFallbackReloadTask();

  void CancelReloadTask();

  // Invoked from the reload task on the FILE thread.
  void ReloadFromTask();

 private:
  friend class AsynchronousPolicyLoaderTest;

  // Finishes loader initialization after the threading system has been fully
  // intialized.
  void InitAfterFileThreadAvailable();

  // Invokes the |update_callback_| with a new PolicyBundle that maps
  // the chrome namespace to |policy|. Must be called on |origin_loop_| so that
  // it's safe to invoke |update_callback_|.
  void UpdatePolicy(scoped_ptr<PolicyBundle> policy);

  // Provides the low-level mechanics for loading policy.
  scoped_ptr<AsynchronousPolicyProvider::Delegate> delegate_;

  // Used to create and invalidate WeakPtrs on the FILE thread. These are only
  // used to post reload tasks that can be cancelled.
  base::WeakPtrFactory<AsynchronousPolicyLoader> weak_ptr_factory_;

  // The interval at which a policy reload will be triggered as a fallback.
  const base::TimeDelta  reload_interval_;

  // The message loop on which this object was constructed. Recorded so that
  // it's possible to call back into the non thread safe provider to fire the
  // notification.
  MessageLoop* origin_loop_;

  // True if Stop has been called.
  bool stopped_;

  // Callback to invoke on policy updates.
  UpdateCallback update_callback_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNCHRONOUS_POLICY_LOADER_H_
