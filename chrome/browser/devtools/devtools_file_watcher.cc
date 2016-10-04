// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_file_watcher.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

static int kFirstThrottleTimeout = 10;
static int kDefaultThrottleTimeout = 200;

// DevToolsFileWatcher::SharedFileWatcher --------------------------------------

class DevToolsFileWatcher::SharedFileWatcher :
    public base::RefCounted<SharedFileWatcher> {
 public:
  SharedFileWatcher();

  void AddListener(DevToolsFileWatcher* watcher);
  void RemoveListener(DevToolsFileWatcher* watcher);
  void AddWatch(const base::FilePath& path);
  void RemoveWatch(const base::FilePath& path);

 private:
  friend class base::RefCounted<
      DevToolsFileWatcher::SharedFileWatcher>;
  ~SharedFileWatcher();

  using FilePathTimesMap = std::map<base::FilePath, base::Time>;
  void GetModificationTimes(const base::FilePath& path,
                            FilePathTimesMap* file_path_times);
  void DirectoryChanged(const base::FilePath& path, bool error);
  void DispatchNotifications();

  std::vector<DevToolsFileWatcher*> listeners_;
  std::map<base::FilePath, std::unique_ptr<base::FilePathWatcher>> watchers_;
  std::map<base::FilePath, FilePathTimesMap> file_path_times_;
  std::set<base::FilePath> pending_paths_;
  base::Time last_event_time_;
  base::TimeDelta last_dispatch_cost_;
};

DevToolsFileWatcher::SharedFileWatcher::SharedFileWatcher()
    : last_dispatch_cost_(
          base::TimeDelta::FromMilliseconds(kDefaultThrottleTimeout)) {
  DevToolsFileWatcher::s_shared_watcher_ = this;
}

DevToolsFileWatcher::SharedFileWatcher::~SharedFileWatcher() {
  DevToolsFileWatcher::s_shared_watcher_ = nullptr;
}

void DevToolsFileWatcher::SharedFileWatcher::AddListener(
    DevToolsFileWatcher* watcher) {
  listeners_.push_back(watcher);
}

void DevToolsFileWatcher::SharedFileWatcher::RemoveListener(
    DevToolsFileWatcher* watcher) {
  auto it = std::find(listeners_.begin(), listeners_.end(), watcher);
  listeners_.erase(it);
}

void DevToolsFileWatcher::SharedFileWatcher::AddWatch(
    const base::FilePath& path) {
  if (watchers_.find(path) != watchers_.end())
    return;
  if (!base::FilePathWatcher::RecursiveWatchAvailable())
    return;
  watchers_[path].reset(new base::FilePathWatcher());
  bool success = watchers_[path]->Watch(
      path, true,
      base::Bind(&SharedFileWatcher::DirectoryChanged, base::Unretained(this)));
  if (!success)
    return;

  GetModificationTimes(path, &file_path_times_[path]);
}

void DevToolsFileWatcher::SharedFileWatcher::GetModificationTimes(
    const base::FilePath& path,
    FilePathTimesMap* times_map) {
  base::FileEnumerator enumerator(path, true, base::FileEnumerator::FILES);
  base::FilePath file_path = enumerator.Next();
  while (!file_path.empty()) {
    base::FileEnumerator::FileInfo file_info = enumerator.GetInfo();
    (*times_map)[file_path] = file_info.GetLastModifiedTime();
    file_path = enumerator.Next();
  }
}

void DevToolsFileWatcher::SharedFileWatcher::RemoveWatch(
    const base::FilePath& path) {
  watchers_.erase(path);
}

void DevToolsFileWatcher::SharedFileWatcher::DirectoryChanged(
    const base::FilePath& path,
    bool error) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  pending_paths_.insert(path);
  if (pending_paths_.size() > 1)
    return;  // PostDelayedTask is already pending.

  base::Time now = base::Time::Now();
  // Quickly dispatch first chunk.
  base::TimeDelta shedule_for =
      now - last_event_time_ > last_dispatch_cost_ ?
          base::TimeDelta::FromMilliseconds(kFirstThrottleTimeout) :
          last_dispatch_cost_ * 2;

  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&DevToolsFileWatcher::SharedFileWatcher::DispatchNotifications,
                 this), shedule_for);
  last_event_time_ = now;
}

void DevToolsFileWatcher::SharedFileWatcher::DispatchNotifications() {
  if (!pending_paths_.size())
    return;
  base::Time start = base::Time::Now();
  std::vector<std::string> added_paths;
  std::vector<std::string> removed_paths;
  std::vector<std::string> changed_paths;

  for (const auto& path : pending_paths_) {
    FilePathTimesMap& old_times = file_path_times_[path];
    FilePathTimesMap current_times;
    GetModificationTimes(path, &current_times);
    for (const auto& path_time : current_times) {
      const base::FilePath& path = path_time.first;
      auto old_timestamp = old_times.find(path);
      if (old_timestamp == old_times.end())
        added_paths.push_back(path.AsUTF8Unsafe());
      else if (old_timestamp->second != path_time.second)
        changed_paths.push_back(path.AsUTF8Unsafe());
    }
    for (const auto& path_time : old_times) {
      const base::FilePath& path = path_time.first;
      if (current_times.find(path) == current_times.end())
        removed_paths.push_back(path.AsUTF8Unsafe());
    }
    old_times.swap(current_times);
  }
  pending_paths_.clear();

  for (auto* watcher : listeners_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(watcher->callback_, changed_paths,
                                       added_paths, removed_paths));
  }
  last_dispatch_cost_ = base::Time::Now() - start;
}

// static
DevToolsFileWatcher::SharedFileWatcher*
DevToolsFileWatcher::s_shared_watcher_ = nullptr;

// DevToolsFileWatcher ---------------------------------------------------------

DevToolsFileWatcher::DevToolsFileWatcher(const WatchCallback& callback)
    : callback_(callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                          base::Bind(&DevToolsFileWatcher::InitSharedWatcher,
                                     base::Unretained(this)));
}

DevToolsFileWatcher::~DevToolsFileWatcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  shared_watcher_->RemoveListener(this);
}

void DevToolsFileWatcher::InitSharedWatcher() {
  if (!DevToolsFileWatcher::s_shared_watcher_)
    new SharedFileWatcher();
  shared_watcher_ = DevToolsFileWatcher::s_shared_watcher_;
  shared_watcher_->AddListener(this);
}

void DevToolsFileWatcher::AddWatch(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  shared_watcher_->AddWatch(path);
}

void DevToolsFileWatcher::RemoveWatch(const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::FILE);
  shared_watcher_->RemoveWatch(path);
}
