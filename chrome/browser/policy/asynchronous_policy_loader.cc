// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_map.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace policy {

AsynchronousPolicyLoader::AsynchronousPolicyLoader(
    AsynchronousPolicyProvider::Delegate* delegate,
    int reload_interval_minutes)
    : delegate_(delegate),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      reload_interval_(base::TimeDelta::FromMinutes(reload_interval_minutes)),
      origin_loop_(MessageLoop::current()),
      stopped_(false) {}

void AsynchronousPolicyLoader::Init(const UpdateCallback& callback) {
  update_callback_ = callback;

  // Load initial policy synchronously at startup.
  scoped_ptr<PolicyMap> policy(delegate_->Load());
  if (policy.get())
    UpdatePolicy(policy.Pass());

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

void AsynchronousPolicyLoader::Reload(bool force) {
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
      delay);
}

void AsynchronousPolicyLoader::ScheduleFallbackReloadTask() {
  // As a safeguard in case that the load delegate failed to timely notice a
  // change in policy, schedule a reload task that'll make us recheck after a
  // reasonable interval.
  ScheduleReloadTask(reload_interval_);
}

void AsynchronousPolicyLoader::ReloadFromTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Reload(false);
}

void AsynchronousPolicyLoader::InitOnFileThread() {
}

void AsynchronousPolicyLoader::StopOnFileThread() {
  delegate_.reset();
  CancelReloadTask();
}

void AsynchronousPolicyLoader::PostUpdatePolicyTask(PolicyMap* new_policy) {
  scoped_ptr<PolicyMap> policy(new_policy);
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&AsynchronousPolicyLoader::UpdatePolicy,
                 this, base::Passed(&policy)));
}

void AsynchronousPolicyLoader::UpdatePolicy(scoped_ptr<PolicyMap> policy) {
  if (!stopped_) {
    // TODO(joaodasilva): make this load policy from other namespaces too.
    scoped_ptr<PolicyBundle> bundle(new PolicyBundle());
    bundle->Get(POLICY_DOMAIN_CHROME, std::string()).Swap(policy.get());
    update_callback_.Run(bundle.Pass());
  }
}

void AsynchronousPolicyLoader::InitAfterFileThreadAvailable() {
  if (!stopped_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&AsynchronousPolicyLoader::InitOnFileThread, this));
  }
}

}  // namespace policy
