// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/config_dir_policy_provider.h"

#include <set>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/json_value_serializer.h"

namespace {

// Amount of time we wait for the files in the policy directory to settle before
// trying to load it. This alleviates the problem of reading partially written
// files and allows to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

// The time interval for rechecking policy. This is our fallback in case the
// directory watch fails or doesn't report a change.
const int kReloadIntervalMinutes = 15;

}  // namespace

// PolicyDirLoader implementation:

PolicyDirLoader::PolicyDirLoader(
    base::WeakPtr<ConfigDirPolicyProvider> provider,
    const FilePath& config_dir,
    int settle_interval_seconds,
    int reload_interval_minutes)
    : provider_(provider),
      origin_loop_(MessageLoop::current()),
      config_dir_(config_dir),
      reload_task_(NULL),
      settle_interval_seconds_(settle_interval_seconds),
      reload_interval_minutes_(reload_interval_minutes) {
  // Force an initial load, so GetPolicy() works.
  policy_.reset(Load());
  DCHECK(policy_.get());
}

void PolicyDirLoader::Stop() {
  if (!ChromeThread::CurrentlyOn(ChromeThread::FILE)) {
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
                           NewRunnableMethod(this, &PolicyDirLoader::Stop));
    return;
  }

  if (reload_task_) {
    reload_task_->Cancel();
    reload_task_ = NULL;
  }
}

void PolicyDirLoader::Reload() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Check the directory time in order to see whether a reload is required.
  base::TimeDelta delay;
  base::Time now = base::Time::Now();
  if (!IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  // Load the policy definitions.
  scoped_ptr<DictionaryValue> new_policy(Load());

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
    LOG(INFO) << "Policy reload from " << config_dir_.value() << " succeeded.";
    origin_loop_->PostTask(FROM_HERE,
        NewRunnableMethod(this, &PolicyDirLoader::NotifyPolicyChanged));
  }

  // As a safeguard in case the file watcher fails, schedule a reload task
  // that'll make us recheck after a reasonable interval.
  ScheduleReloadTask(base::TimeDelta::FromMinutes(reload_interval_minutes_));
}

DictionaryValue* PolicyDirLoader::GetPolicy() {
  AutoLock lock(lock_);
  return static_cast<DictionaryValue*>(policy_->DeepCopy());
}

void PolicyDirLoader::OnFilePathChanged(const FilePath& path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
  Reload();
}

void PolicyDirLoader::OnError() {
  LOG(ERROR) << "FileWatcher on " << config_dir_.value() << " failed.";
}

DictionaryValue* PolicyDirLoader::Load() {
  // Enumerate the files and sort them lexicographically.
  std::set<FilePath> files;
  file_util::FileEnumerator file_enumerator(config_dir_, false,
                                            file_util::FileEnumerator::FILES);
  for (FilePath config_file_path = file_enumerator.Next();
       !config_file_path.empty(); config_file_path = file_enumerator.Next())
    files.insert(config_file_path);

  // Start with an empty dictionary and merge the files' contents.
  DictionaryValue* policy = new DictionaryValue;
  for (std::set<FilePath>::iterator config_file_iter = files.begin();
       config_file_iter != files.end(); ++config_file_iter) {
    JSONFileValueSerializer deserializer(*config_file_iter);
    int error_code = 0;
    std::string error_msg;
    scoped_ptr<Value> value(deserializer.Deserialize(&error_code, &error_msg));
    if (!value.get()) {
      LOG(WARNING) << "Failed to read configuration file "
                   << config_file_iter->value() << ": " << error_msg;
      continue;
    }
    if (!value->IsType(Value::TYPE_DICTIONARY)) {
      LOG(WARNING) << "Expected JSON dictionary in configuration file "
                   << config_file_iter->value();
      continue;
    }
    policy->MergeDictionary(static_cast<DictionaryValue*>(value.get()));
  }

  return policy;
}

bool PolicyDirLoader::IsSafeToReloadPolicy(const base::Time& now,
                                           base::TimeDelta* delay) {
  DCHECK(delay);
  base::PlatformFileInfo dir_info;

  // Reading an empty directory or a file is always safe.
  if (!file_util::GetFileInfo(config_dir_, &dir_info) ||
      !dir_info.is_directory) {
    last_modification_file_ = base::Time();
    return true;
  }

  // If there was a change since the last recorded modification, wait some more.
  base::TimeDelta settleInterval(
      base::TimeDelta::FromSeconds(settle_interval_seconds_));
  if (dir_info.last_modified != last_modification_file_) {
    last_modification_file_ = dir_info.last_modified;
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

void PolicyDirLoader::ScheduleReloadTask(const base::TimeDelta& delay) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  if (reload_task_)
    reload_task_->Cancel();

  reload_task_ = NewRunnableMethod(this, &PolicyDirLoader::ReloadFromTask);
  ChromeThread::PostDelayedTask(ChromeThread::FILE, FROM_HERE, reload_task_,
                                delay.InMilliseconds());
}

void PolicyDirLoader::NotifyPolicyChanged() {
  DCHECK_EQ(origin_loop_, MessageLoop::current());
  if (provider_)
    provider_->NotifyStoreOfPolicyChange();
}

void PolicyDirLoader::ReloadFromTask() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));

  // Drop the reference to the reload task, since the task might be the only
  // referer that keeps us alive, so we should not Cancel() it.
  reload_task_ = NULL;

  Reload();
}

// PolicyDirWatcher implementation:

void PolicyDirWatcher::Init(PolicyDirLoader* loader) {
  // Initialization can happen early when the file thread is not yet available.
  // So post a task to ourselves on th UI thread which will run after threading
  // is up and schedule watch initialization on the file thread.
  ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyDirWatcher::InitWatcher,
                        scoped_refptr<PolicyDirLoader>(loader)));
}

void PolicyDirWatcher::InitWatcher(
    const scoped_refptr<PolicyDirLoader>& loader) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::FILE)) {
    ChromeThread::PostTask(ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &PolicyDirWatcher::InitWatcher, loader));
    return;
  }

  if (!Watch(loader->config_dir(), loader.get()))
    loader->OnError();

  // There might have been changes to the directory in the time between
  // construction of the loader and initialization of the watcher. Call reload
  // to detect if that is the case.
  loader->Reload();
}

// ConfigDirPolicyProvider implementation:

ConfigDirPolicyProvider::ConfigDirPolicyProvider(const FilePath& config_dir) {
  loader_ = new PolicyDirLoader(AsWeakPtr(), config_dir, kSettleIntervalSeconds,
                                kReloadIntervalMinutes);
  watcher_ = new PolicyDirWatcher;
  watcher_->Init(loader_.get());
}

ConfigDirPolicyProvider::~ConfigDirPolicyProvider() {
  loader_->Stop();
}

bool ConfigDirPolicyProvider::Provide(ConfigurationPolicyStore* store) {
  scoped_ptr<DictionaryValue> policy(loader_->GetPolicy());
  DCHECK(policy.get());
  DecodePolicyValueTree(policy.get(), store);
  return true;
}

void ConfigDirPolicyProvider::DecodePolicyValueTree(
    DictionaryValue* policies,
    ConfigurationPolicyStore* store) {
  const PolicyValueMap* mapping = PolicyValueMapping();
  for (PolicyValueMap::const_iterator i = mapping->begin();
       i != mapping->end(); ++i) {
    const PolicyValueMapEntry& entry(*i);
    Value* value;
    if (policies->Get(entry.name, &value) && value->IsType(entry.value_type))
      store->Apply(entry.policy_type, value->DeepCopy());
  }

  // TODO(mnissler): Handle preference overrides once |ConfigurationPolicyStore|
  // supports it.
}
