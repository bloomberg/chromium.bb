// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/asynchronous_policy_loader.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "content/browser/browser_thread.h"

namespace policy {

AsynchronousPolicyLoader::AsynchronousPolicyLoader(
    AsynchronousPolicyProvider::Delegate* delegate,
    int reload_interval_minutes)
    : delegate_(delegate),
      reload_task_(NULL),
      reload_interval_(base::TimeDelta::FromMinutes(reload_interval_minutes)),
      origin_loop_(MessageLoop::current()),
      stopped_(false) {}

void AsynchronousPolicyLoader::Init() {
  policy_.reset(delegate_->Load());
  // Initialization can happen early when the file thread is not yet available,
  // but the subclass of the loader must do some of their initialization on the
  // file thread. Posting to the file thread directly before it is initialized
  // will cause the task to be forgotten. Instead, post a task to the ui thread
  // to delay the remainder of initialization until threading is fully
  // initialized.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(
          this,
          &AsynchronousPolicyLoader::InitAfterFileThreadAvailable));
}

void AsynchronousPolicyLoader::Stop() {
  if (!stopped_) {
    stopped_ = true;
    delegate_.reset();
    FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                      observer_list_,
                      OnProviderGoingAway());
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &AsynchronousPolicyLoader::StopOnFileThread));
  }
}

AsynchronousPolicyLoader::~AsynchronousPolicyLoader() {
}

// Manages the life cycle of a new policy map during until its life cycle is
// taken over by the policy loader.
class UpdatePolicyTask : public Task {
 public:
  UpdatePolicyTask(scoped_refptr<AsynchronousPolicyLoader> loader,
                   DictionaryValue* new_policy)
      : loader_(loader),
        new_policy_(new_policy) {}

  virtual void Run() {
    loader_->UpdatePolicy(new_policy_.release());
  }

 private:
  scoped_refptr<AsynchronousPolicyLoader> loader_;
  scoped_ptr<DictionaryValue> new_policy_;
  DISALLOW_COPY_AND_ASSIGN(UpdatePolicyTask);
};

void AsynchronousPolicyLoader::Reload() {
  if (delegate_.get()) {
    DictionaryValue* new_policy = delegate_->Load();
    PostUpdatePolicyTask(new_policy);
  }
}

void AsynchronousPolicyLoader::AddObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AsynchronousPolicyLoader::RemoveObserver(
    ConfigurationPolicyProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AsynchronousPolicyLoader::CancelReloadTask() {
  if (reload_task_) {
    // Only check the thread if there's still a reload task. During
    // destruction of unit tests, the message loop destruction can
    // call this method when the file thread is no longer around,
    // but in that case reload_task_ is NULL.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    reload_task_->Cancel();
    reload_task_ = NULL;
  }
}

void AsynchronousPolicyLoader::ScheduleReloadTask(
    const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  CancelReloadTask();

  reload_task_ =
      NewRunnableMethod(this, &AsynchronousPolicyLoader::ReloadFromTask);
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE, reload_task_,
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

  // Drop the reference to the reload task, since the task might be the only
  // referrer that keeps us alive, so we should not Cancel() it.
  reload_task_ = NULL;

  Reload();
}

void AsynchronousPolicyLoader::InitOnFileThread() {
}

void AsynchronousPolicyLoader::StopOnFileThread() {
  CancelReloadTask();
}

void AsynchronousPolicyLoader::PostUpdatePolicyTask(
    DictionaryValue* new_policy) {
  origin_loop_->PostTask(FROM_HERE, new UpdatePolicyTask(this, new_policy));
}

void AsynchronousPolicyLoader::UpdatePolicy(DictionaryValue* new_policy_raw) {
  scoped_ptr<DictionaryValue> new_policy(new_policy_raw);
  DCHECK(policy_.get());
  if (!policy_->Equals(new_policy.get())) {
    policy_.reset(new_policy.release());
    FOR_EACH_OBSERVER(ConfigurationPolicyProvider::Observer,
                      observer_list_,
                      OnUpdatePolicy());
  }
}

void AsynchronousPolicyLoader::InitAfterFileThreadAvailable() {
  if (!stopped_) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &AsynchronousPolicyLoader::InitOnFileThread));
  }
}

}  // namespace policy
