// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/file_based_policy_loader.h"

namespace {

// Amount of time we wait for the files on disk to settle before trying to load
// them. This alleviates the problem of reading partially written files and
// makes it possible to batch quasi-simultaneous changes.
const int kSettleIntervalSeconds = 5;

// The time interval for rechecking policy. This is our fallback in case the
// delegate never reports a change to the ReloadObserver.
const int kReloadIntervalMinutes = 15;

}

namespace policy {

FileBasedPolicyLoader::FileBasedPolicyLoader(
    FileBasedPolicyProvider::ProviderDelegate* provider_delegate)
    : AsynchronousPolicyLoader(provider_delegate),
      config_file_path_(provider_delegate->config_file_path()),
      reload_task_(NULL),
      reload_interval_(base::TimeDelta::FromMinutes(kReloadIntervalMinutes)),
      settle_interval_(base::TimeDelta::FromSeconds(kSettleIntervalSeconds)) {
}

class FileBasedPolicyWatcherDelegate : public FilePathWatcher::Delegate {
 public:
  explicit FileBasedPolicyWatcherDelegate(
      scoped_refptr<FileBasedPolicyLoader> loader)
      : loader_(loader) {}
  virtual ~FileBasedPolicyWatcherDelegate() {}

  // FilePathWatcher::Delegate implementation:
  void OnFilePathChanged(const FilePath& path) {
    loader_->OnFilePathChanged(path);
  }

  void OnError() {
    loader_->OnError();
  }

 private:
  scoped_refptr<FileBasedPolicyLoader> loader_;
  DISALLOW_COPY_AND_ASSIGN(FileBasedPolicyWatcherDelegate);
};

void FileBasedPolicyLoader::Init() {
  AsynchronousPolicyLoader::Init();

  // Initialization can happen early when the file thread is not yet available,
  // but the watcher's initialization must be done on the file thread. Posting
  // to a to the file directly before the file thread is initialized will cause
  // the task to be forgotten. Instead, post a task to the ui thread to delay
  // the remainder of initialization until threading is fully initialized.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &FileBasedPolicyLoader::InitAfterFileThreadAvailable));
}

void FileBasedPolicyLoader::Stop() {
  AsynchronousPolicyLoader::Stop();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &FileBasedPolicyLoader::StopOnFileThread));
}

void FileBasedPolicyLoader::OnFilePathChanged(
    const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Reload();
}

void FileBasedPolicyLoader::OnError() {
  LOG(ERROR) << "FilePathWatcher on " << config_file_path().value()
             << " failed.";
}

FileBasedPolicyLoader::~FileBasedPolicyLoader() {}

void FileBasedPolicyLoader::Reload() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!delegate())
    return;

  // Check the directory time in order to see whether a reload is required.
  base::TimeDelta delay;
  base::Time now = base::Time::Now();
  if (!IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  // Load the policy definitions.
  scoped_ptr<DictionaryValue> new_policy(delegate()->Load());

  // Check again in case the directory has changed while reading it.
  if (!IsSafeToReloadPolicy(now, &delay)) {
    ScheduleReloadTask(delay);
    return;
  }

  PostUpdatePolicyTask(new_policy.release());

  ScheduleFallbackReloadTask();
}

void FileBasedPolicyLoader::InitAfterFileThreadAvailable() {
  if (provider()) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(this, &FileBasedPolicyLoader::InitOnFileThread));
  }
}

void FileBasedPolicyLoader::InitOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  watcher_.reset(new FilePathWatcher);
  if (!config_file_path().empty() &&
      !watcher_->Watch(config_file_path(),
                       new FileBasedPolicyWatcherDelegate(this))) {
    OnError();
  }

  // There might have been changes to the directory in the time between
  // construction of the loader and initialization of the watcher. Call reload
  // to detect if that is the case.
  Reload();

  ScheduleFallbackReloadTask();
}

void FileBasedPolicyLoader::StopOnFileThread() {
  watcher_.reset();
  if (reload_task_) {
    reload_task_->Cancel();
    reload_task_ = NULL;
  }
}

void FileBasedPolicyLoader::ScheduleReloadTask(
    const base::TimeDelta& delay) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (reload_task_)
    reload_task_->Cancel();

  reload_task_ =
      NewRunnableMethod(this, &FileBasedPolicyLoader::ReloadFromTask);
  BrowserThread::PostDelayedTask(BrowserThread::FILE, FROM_HERE, reload_task_,
                                 delay.InMilliseconds());
}

void FileBasedPolicyLoader::ScheduleFallbackReloadTask() {
  // As a safeguard in case that the load delegate failed to timely notice a
  // change in policy, schedule a reload task that'll make us recheck after a
  // reasonable interval.
  ScheduleReloadTask(reload_interval_);
}

void FileBasedPolicyLoader::ReloadFromTask() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Drop the reference to the reload task, since the task might be the only
  // referer that keeps us alive, so we should not Cancel() it.
  reload_task_ = NULL;

  Reload();
}

bool FileBasedPolicyLoader::IsSafeToReloadPolicy(
    const base::Time& now,
    base::TimeDelta* delay) {
  DCHECK(delay);

  // A null modification time indicates there's no data.
  FileBasedPolicyProvider::ProviderDelegate* provider_delegate =
      static_cast<FileBasedPolicyProvider::ProviderDelegate*>(delegate());
  base::Time last_modification(provider_delegate->GetLastModification());
  if (last_modification.is_null())
    return true;

  // If there was a change since the last recorded modification, wait some more.
  if (last_modification != last_modification_file_) {
    last_modification_file_ = last_modification;
    last_modification_clock_ = now;
    *delay = settle_interval_;
    return false;
  }

  // Check whether the settle interval has elapsed.
  base::TimeDelta age = now - last_modification_clock_;
  if (age < settle_interval_) {
    *delay = settle_interval_ - age;
    return false;
  }

  return true;
}

}  // namespace policy
