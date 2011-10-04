// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread.h"

namespace policy {

AsynchronousPolicyLoader::AsynchronousPolicyLoader(
    AsynchronousPolicyProvider::Delegate* delegate,
    int reload_interval_minutes)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      reload_interval_(base::TimeDelta::FromMinutes(reload_interval_minutes)),
      origin_loop_(MessageLoop::current()),
      stopped_(false) {}

void AsynchronousPolicyLoader::Init(const base::Closure& callback) {
  updates_callback_ = callback;
  policy_.reset(delegate_->Load());
  // Initialization can happen early when the file thread is not yet available,
  // but the subclass of the loader must do some of their initialization on the
  // file thread. Posting to the file thread directly before it is initialized
  // will cause the task to be forgotten. Instead, post a task to the ui thread
  // to delay the remainder of initialization until threading is fully
  // initialized.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AsynchronousPolicyLoader::InitAfterFileThreadAvailable,
                 this));
}

void AsynchronousPolicyLoader::Stop() {
  if (!stopped_) {
    stopped_ = true;
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&AsynchronousPolicyLoader::StopOnFileThread, this));
  }
}

AsynchronousPolicyLoader::~AsynchronousPolicyLoader() {
}

void AsynchronousPolicyLoader::Reload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (delegate_.get()) {
    PostUpdatePolicyTask(delegate_->Load());
  }
}

void AsynchronousPolicyLoader::CancelReloadTask() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AsynchronousPolicyLoader::ScheduleReloadTask(
    const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  CancelReloadTask();

  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&AsynchronousPolicyLoader::ReloadFromTask,
                 weak_ptr_factory_.GetWeakPtr()),
      delay.InMilliseconds());
}

void AsynchronousPolicyLoader::ScheduleFallbackReloadTask() {
  // As a safeguard in case that the load delegate failed to timely notice a
  // change in policy, schedule a reload task that'll make us recheck after a
  // reasonable interval.
  ScheduleReloadTask(reload_interval_);
}

void AsynchronousPolicyLoader::ReloadFromTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Reload();
}

void AsynchronousPolicyLoader::InitOnFileThread() {
}

void AsynchronousPolicyLoader::StopOnFileThread() {
  delegate_.reset();
  CancelReloadTask();
}

void AsynchronousPolicyLoader::PostUpdatePolicyTask(
    DictionaryValue* new_policy) {
  // TODO(joaodasilva): make the callback own |new_policy|.
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AsynchronousPolicyLoader::UpdatePolicy, this, new_policy));
}

void AsynchronousPolicyLoader::UpdatePolicy(DictionaryValue* new_policy_raw) {
  scoped_ptr<DictionaryValue> new_policy(new_policy_raw);
  DCHECK(policy_.get());
  policy_.swap(new_policy);
  if (!stopped_)
    updates_callback_.Run();
}

void AsynchronousPolicyLoader::InitAfterFileThreadAvailable() {
  if (!stopped_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&AsynchronousPolicyLoader::InitOnFileThread, this));
  }
}

}  // namespace policy
