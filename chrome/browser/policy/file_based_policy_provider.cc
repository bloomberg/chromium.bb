// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_based_policy_provider.h"

#include <set>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/json_value_serializer.h"

namespace policy {

// Amount of time we wait for the files on disk to settle before trying to load
// it. This alleviates the problem of reading partially written files and allows
// to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

// The time interval for rechecking policy. This is our fallback in case the
// file path watch fails or doesn't report a change.
const int kReloadIntervalMinutes = 15;

// FileBasedPolicyProvider implementation:

FileBasedPolicyProvider::Delegate::~Delegate() {
}

FileBasedPolicyProvider::Delegate::Delegate(const FilePath& config_file_path)
  : config_file_path_(config_file_path) {
}

FileBasedPolicyProvider::FileBasedPolicyProvider(
    const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list,
    FileBasedPolicyProvider::Delegate* delegate)
    : ConfigurationPolicyProvider(policy_list) {
  loader_ = new FileBasedPolicyLoader(AsWeakPtr(),
                                      delegate,
                                      kSettleIntervalSeconds,
                                      kReloadIntervalMinutes);
  watcher_ = new FileBasedPolicyWatcher;
  watcher_->Init(loader_.get());
}

FileBasedPolicyProvider::~FileBasedPolicyProvider() {
  loader_->Stop();
}

bool FileBasedPolicyProvider::Provide(
    ConfigurationPolicyStoreInterface* store) {
  scoped_ptr<DictionaryValue> policy(loader_->GetPolicy());
  DCHECK(policy.get());
  DecodePolicyValueTree(policy.get(), store);
  return true;
}

void FileBasedPolicyProvider::DecodePolicyValueTree(
    DictionaryValue* policies,
    ConfigurationPolicyStoreInterface* store) {
  const PolicyDefinitionList* policy_list(policy_definition_list());
  for (const PolicyDefinitionList::Entry* i = policy_list->begin;
       i != policy_list->end; ++i) {
    Value* value;
    if (policies->Get(i->name, &value) && value->IsType(i->value_type))
      store->Apply(i->policy_type, value->DeepCopy());
  }

  // TODO(mnissler): Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}

// FileBasedPolicyLoader implementation:

FileBasedPolicyLoader::FileBasedPolicyLoader(
    base::WeakPtr<ConfigurationPolicyProvider> provider,
    FileBasedPolicyProvider::Delegate* delegate,
    int settle_interval_seconds,
    int reload_interval_minutes)
    : delegate_(delegate),
      provider_(provider),
      origin_loop_(MessageLoop::current()),
      reload_task_(NULL),
      settle_interval_seconds_(settle_interval_seconds),
      reload_interval_minutes_(reload_interval_minutes) {
  // Force an initial load, so GetPolicy() works.
  policy_.reset(delegate_->Load());
  DCHECK(policy_.get());
}

void FileBasedPolicyLoader::Stop() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &FileBasedPolicyLoader::Stop));
    return;
  }

  if (reload_task_) {
    reload_task_->Cancel();
    reload_task_ = NULL;
  }
}

void FileBasedPolicyLoader::Reload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Check the directory time in order to see whether a reload is required.
  base::TimeDelta delay;
  base::Time now = base::Time::Now();
  if (!IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  // Load the policy definitions.
  scoped_ptr<DictionaryValue> new_policy(delegate_->Load());

  // Check again in case the directory has changed while reading it.
  if (!IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  // Replace policy definition.
  bool changed = false;
  {
    AutoLock lock(lock_);
    changed = !policy_->Equals(new_policy.get());
    policy_.reset(new_policy.release());
  }

  // There's a change, report it!
  if (changed) {
    VLOG(0) << "Policy reload from " << config_file_path().value()
            << " succeeded.";
    origin_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &FileBasedPolicyLoader::NotifyPolicyChanged));
  }

  // As a safeguard in case the file watcher fails, schedule a reload task
  // that'll make us recheck after a reasonable interval.
  ScheduleReloadTask(base::TimeDelta::FromMinutes(reload_interval_minutes_));
}

DictionaryValue* FileBasedPolicyLoader::GetPolicy() {
  AutoLock lock(lock_);
  return static_cast<DictionaryValue*>(policy_->DeepCopy());
}

void FileBasedPolicyLoader::OnFilePathChanged(const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Reload();
}

void FileBasedPolicyLoader::OnError() {
  LOG(ERROR) << "FilePathWatcher on " << config_file_path().value()
             << " failed.";
}

FileBasedPolicyLoader::~FileBasedPolicyLoader() {
}

bool FileBasedPolicyLoader::IsSafeToReloadPolicy(const base::Time& now,
                                                 base::TimeDelta* delay) {
  DCHECK(delay);

  // A null modification time indicates there's no data.
  base::Time last_modification(delegate_->GetLastModification());
  if (last_modification.is_null())
    return true;

  // If there was a change since the last recorded modification, wait some more.
  base::TimeDelta settleInterval(
      base::TimeDelta::FromSeconds(settle_interval_seconds_));
  if (last_modification != last_modification_file_) {
    last_modification_file_ = last_modification;
    last_modification_clock_ = now;
    *delay = settleInterval;
    return false;
  }

  // Check whether the settle interval has elapsed.
  base::TimeDelta age = now - last_modification_clock_;
  if (age < settleInterval) {
    *delay = settleInterval - age;
    return false;
  }

  return true;
}

void FileBasedPolicyLoader::ScheduleReloadTask(const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (reload_task_)
    reload_task_->Cancel();

  reload_task_ =
      NewRunnableMethod(this, &FileBasedPolicyLoader::ReloadFromTask);
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE, reload_task_,
                                 delay.InMilliseconds());
}

void FileBasedPolicyLoader::NotifyPolicyChanged() {
  DCHECK_EQ(origin_loop_, MessageLoop::current());
  if (provider_)
    provider_->NotifyStoreOfPolicyChange();
}

void FileBasedPolicyLoader::ReloadFromTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Drop the reference to the reload task, since the task might be the only
  // referer that keeps us alive, so we should not Cancel() it.
  reload_task_ = NULL;

  Reload();
}

// FileBasedPolicyWatcher implementation:

FileBasedPolicyWatcher::FileBasedPolicyWatcher() {
}

void FileBasedPolicyWatcher::Init(FileBasedPolicyLoader* loader) {
  // Initialization can happen early when the file thread is not yet available.
  // So post a task to ourselves on the UI thread which will run after threading
  // is up and schedule watch initialization on the file thread.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &FileBasedPolicyWatcher::InitWatcher,
                        scoped_refptr<FileBasedPolicyLoader>(loader)));
}

FileBasedPolicyWatcher::~FileBasedPolicyWatcher() {
}

void FileBasedPolicyWatcher::InitWatcher(
    const scoped_refptr<FileBasedPolicyLoader>& loader) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &FileBasedPolicyWatcher::InitWatcher, loader));
    return;
  }

  if (!loader->config_file_path().empty() &&
      !watcher_.Watch(loader->config_file_path(), loader.get()))
    loader->OnError();

  // There might have been changes to the directory in the time between
  // construction of the loader and initialization of the watcher. Call reload
  // to detect if that is the case.
  loader->Reload();
}

}  // namespace policy
