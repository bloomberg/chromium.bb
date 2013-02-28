// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/async_policy_loader.h"

#include "base/bind.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "content/public/browser/browser_thread.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace policy {

namespace {

// Amount of time to wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

// The time interval for rechecking policy. This is the fallback in case the
// implementation never detects changes.
const int kReloadIntervalSeconds = 15 * 60;

}  // namespace

AsyncPolicyLoader::AsyncPolicyLoader()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

AsyncPolicyLoader::~AsyncPolicyLoader() {}

base::Time AsyncPolicyLoader::LastModificationTime() {
  return base::Time();
}

void AsyncPolicyLoader::Reload(bool force) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  TimeDelta delay;
  Time now = Time::Now();
  // Check if there was a recent modification to the underlying files.
  if (!force && !IsSafeToReload(now, &delay)) {
    ScheduleNextReload(delay);
    return;
  }

  scoped_ptr<PolicyBundle> bundle(Load());

  // Check if there was a modification while reading.
  if (!force && !IsSafeToReload(now, &delay)) {
    ScheduleNextReload(delay);
    return;
  }

  update_callback_.Run(bundle.Pass());
  ScheduleNextReload(TimeDelta::FromSeconds(kReloadIntervalSeconds));
}

scoped_ptr<PolicyBundle> AsyncPolicyLoader::InitialLoad() {
  // This is the first load, early during startup. Use this to record the
  // initial |last_modification_time_|, so that potential changes made before
  // installing the watches can be detected.
  last_modification_time_ = LastModificationTime();
  return Load();
}

void AsyncPolicyLoader::Init(const UpdateCallback& update_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(update_callback_.is_null());
  DCHECK(!update_callback.is_null());
  update_callback_ = update_callback;

  InitOnFile();

  // There might have been changes to the underlying files since the initial
  // load and before the watchers have been created.
  if (LastModificationTime() != last_modification_time_)
    Reload(false);

  // Start periodic refreshes.
  ScheduleNextReload(TimeDelta::FromSeconds(kReloadIntervalSeconds));
}

void AsyncPolicyLoader::ScheduleNextReload(TimeDelta delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  weak_factory_.InvalidateWeakPtrs();
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsyncPolicyLoader::Reload,
                 weak_factory_.GetWeakPtr(),
                 false  /* force */),
      delay);
}

bool AsyncPolicyLoader::IsSafeToReload(const Time& now, TimeDelta* delay) {
  Time last_modification = LastModificationTime();
  if (last_modification.is_null())
    return true;

  // If there was a change since the last recorded modification, wait some more.
  const TimeDelta kSettleInterval(
      TimeDelta::FromSeconds(kSettleIntervalSeconds));
  if (last_modification != last_modification_time_) {
    last_modification_time_ = last_modification;
    last_modification_clock_ = now;
    *delay = kSettleInterval;
    return false;
  }

  // Check whether the settle interval has elapsed.
  const base::TimeDelta age = now - last_modification_clock_;
  if (age < kSettleInterval) {
    *delay = kSettleInterval - age;
    return false;
  }

  return true;
}

}  // namespace policy
