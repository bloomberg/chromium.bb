// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_ASYNC_POLICY_LOADER_H_
#define CHROME_BROWSER_POLICY_ASYNC_POLICY_LOADER_H_
#pragma once

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"

namespace policy {

class PolicyBundle;

// Base implementation for platform-specific policy loaders. Together with the
// AsyncPolicyProvider, this base implementation takes care of the initial load,
// periodic reloads, watching file changes, refreshing policies and object
// lifetime.
//
// All methods are invoked on the FILE thread, including the destructor.
// The only exceptions are the constructor (which may be called on any thread),
// and the initial Load() which is called on the thread that owns the provider.
// LastModificationTime() is also invoked once on that thread at startup.
class AsyncPolicyLoader {
 public:
  AsyncPolicyLoader();
  virtual ~AsyncPolicyLoader();

  // Returns the currently configured policies. Load() is always invoked on
  // the FILE thread, except for the initial Load() at startup which is invoked
  // from the thread that owns the provider.
  virtual scoped_ptr<PolicyBundle> Load() = 0;

  // Allows implementations to finalize their initialization on the FILE
  // thread (e.g. setup file watchers).
  virtual void InitOnFile() = 0;

  // Implementations should return the time of the last modification detected,
  // or base::Time() if it doesn't apply, which is the default.
  virtual base::Time LastModificationTime();

  // Implementations should invoke Reload() when a change is detected. This
  // must be invoked from the FILE thread and will trigger a Load(), and pass
  // the returned bundle to the provider.
  // The load is immediate when |force| is true. Otherwise, the loader
  // reschedules the reload until the LastModificationTime() is a couple of
  // seconds in the past. This mitigates the problem of reading files that are
  // currently being written to, and whose contents are incomplete.
  // A reload is posted periodically, if it hasn't been triggered recently. This
  // makes sure the policies are reloaded if the update events aren't triggered.
  void Reload(bool force);

 private:
  // Allow AsyncPolicyProvider to call Init().
  friend class AsyncPolicyProvider;

  typedef base::Callback<void(scoped_ptr<PolicyBundle>)> UpdateCallback;

  // Used by the AsyncPolicyProvider to do the initial Load(). The first load
  // is also used to initialize |last_modification_time_|.
  scoped_ptr<PolicyBundle> InitialLoad();

  // Used by the AsyncPolicyProvider to install the |update_callback_|.
  // Invoked on the FILE thread.
  void Init(const UpdateCallback& update_callback);

  // Cancels any pending periodic reload and posts one |delay| time units from
  // now.
  void ScheduleNextReload(base::TimeDelta delay);

  // Checks if the underlying files haven't changed recently, by checking the
  // LastModificationTime(). |delay| is updated with a suggested time to wait
  // before retrying when this returns false.
  bool IsSafeToReload(const base::Time& now, base::TimeDelta* delay);

  // Callback for updates, passed in Init().
  UpdateCallback update_callback_;

  // Used to get WeakPtrs for the periodic reload task.
  base::WeakPtrFactory<AsyncPolicyLoader> weak_factory_;

  // Records last known modification timestamp.
  base::Time last_modification_time_;

  // The wall clock time at which the last modification timestamp was
  // recorded.  It's better to not assume the file notification time and the
  // wall clock times come from the same source, just in case there is some
  // non-local filesystem involved.
  base::Time last_modification_clock_;

  DISALLOW_COPY_AND_ASSIGN(AsyncPolicyLoader);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_ASYNC_POLICY_LOADER_H_
