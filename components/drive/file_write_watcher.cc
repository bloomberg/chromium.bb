// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/file_write_watcher.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "google_apis/drive/task_util.h"

namespace drive {
namespace internal {

namespace {
const int64_t kWriteEventDelayInSeconds = 5;
}  // namespace

// base::FileWatcher needs to live in a thread that is allowed to do File IO
// and has a TYPE_IO message loop: that is, FILE thread. This class bridges the
// UI thread and FILE thread, and does all the main tasks in the FILE thread.
class FileWriteWatcher::FileWriteWatcherImpl {
 public:
  explicit FileWriteWatcherImpl(base::SingleThreadTaskRunner* file_task_runner);

  // Forwards the call to DestoryOnFileThread(). This method must be used to
  // destruct the instance.
  void Destroy();

  // Forwards the call to StartWatchOnFileThread(). |on_start_callback| is
  // called back on the caller (UI) thread when the watch has started.
  // |on_write_callback| is called when a write has happened to the path.
  void StartWatch(const base::FilePath& path,
                  const StartWatchCallback& on_start_callback,
                  const base::Closure& on_write_callback);

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

 private:
  ~FileWriteWatcherImpl();

  void DestroyOnFileThread();

  void StartWatchOnFileThread(const base::FilePath& path,
                              const StartWatchCallback& on_start_callback,
                              const base::Closure& on_write_callback);

  void OnWriteEvent(const base::FilePath& path, bool error);

  void InvokeCallback(const base::FilePath& path);

  struct PathWatchInfo {
    std::vector<base::Closure> on_write_callbacks;
    base::FilePathWatcher watcher;
    base::Timer timer;

    explicit PathWatchInfo(const base::Closure& on_write_callback)
        : on_write_callbacks(1, on_write_callback),
          timer(false /* retain_closure_on_reset */, false /* is_repeating */) {
    }
  };

  base::TimeDelta delay_;
  std::map<base::FilePath, std::unique_ptr<PathWatchInfo>> watchers_;
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  base::ThreadChecker thread_checker_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWriteWatcherImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileWriteWatcherImpl);
};

FileWriteWatcher::FileWriteWatcherImpl::FileWriteWatcherImpl(
    base::SingleThreadTaskRunner* file_task_runner)
    : delay_(base::TimeDelta::FromSeconds(kWriteEventDelayInSeconds)),
      file_task_runner_(file_task_runner),
      weak_ptr_factory_(this) {
}

void FileWriteWatcher::FileWriteWatcherImpl::Destroy() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Just forwarding the call to FILE thread.
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileWriteWatcherImpl::DestroyOnFileThread,
                            base::Unretained(this)));
}

void FileWriteWatcher::FileWriteWatcherImpl::StartWatch(
    const base::FilePath& path,
    const StartWatchCallback& on_start_callback,
    const base::Closure& on_write_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Forwarding the call to FILE thread and relaying the |callback|.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FileWriteWatcherImpl::StartWatchOnFileThread,
                 base::Unretained(this), path,
                 google_apis::CreateRelayCallback(on_start_callback),
                 google_apis::CreateRelayCallback(on_write_callback)));
}

FileWriteWatcher::FileWriteWatcherImpl::~FileWriteWatcherImpl() {
  DCHECK(file_task_runner_->BelongsToCurrentThread());
}

void FileWriteWatcher::FileWriteWatcherImpl::DestroyOnFileThread() {
  DCHECK(file_task_runner_->BelongsToCurrentThread());

  delete this;
}

void FileWriteWatcher::FileWriteWatcherImpl::StartWatchOnFileThread(
    const base::FilePath& path,
    const StartWatchCallback& on_start_callback,
    const base::Closure& on_write_callback) {
  DCHECK(file_task_runner_->BelongsToCurrentThread());

  auto it = watchers_.find(path);
  if (it != watchers_.end()) {
    // We are already watching the path.
    on_start_callback.Run(true);
    it->second->on_write_callbacks.push_back(on_write_callback);
    return;
  }

  // Start watching |path|.
  std::unique_ptr<PathWatchInfo> info =
      base::MakeUnique<PathWatchInfo>(on_write_callback);
  bool ok = info->watcher.Watch(
      path,
      false,  // recursive
      base::Bind(&FileWriteWatcherImpl::OnWriteEvent,
                 weak_ptr_factory_.GetWeakPtr()));
  watchers_[path] = std::move(info);
  on_start_callback.Run(ok);
}

void FileWriteWatcher::FileWriteWatcherImpl::OnWriteEvent(
    const base::FilePath& path,
    bool error) {
  DCHECK(file_task_runner_->BelongsToCurrentThread());

  if (error)
    return;

  auto it = watchers_.find(path);
  DCHECK(it != watchers_.end());

  // Heuristics for detecting the end of successive write operations.
  // Delay running on_write_event_callback by |delay_| time, and if OnWriteEvent
  // is called again in the period, the timer is reset. In other words, we
  // invoke callback when |delay_| has passed after the last OnWriteEvent().
  it->second->timer.Start(FROM_HERE,
                          delay_,
                          base::Bind(&FileWriteWatcherImpl::InvokeCallback,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     path));
}

void FileWriteWatcher::FileWriteWatcherImpl::InvokeCallback(
    const base::FilePath& path) {
  DCHECK(file_task_runner_->BelongsToCurrentThread());

  auto it = watchers_.find(path);
  DCHECK(it != watchers_.end());

  std::vector<base::Closure> callbacks;
  callbacks.swap(it->second->on_write_callbacks);
  watchers_.erase(it);

  for (const auto& callback : callbacks)
    callback.Run();
}

FileWriteWatcher::FileWriteWatcher(
    base::SingleThreadTaskRunner* file_task_runner)
    : watcher_impl_(new FileWriteWatcherImpl(file_task_runner)) {
}

FileWriteWatcher::~FileWriteWatcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void FileWriteWatcher::StartWatch(const base::FilePath& file_path,
                                  const StartWatchCallback& on_start_callback,
                                  const base::Closure& on_write_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  watcher_impl_->StartWatch(file_path, on_start_callback, on_write_callback);
}

void FileWriteWatcher::DisableDelayForTesting() {
  watcher_impl_->set_delay(base::TimeDelta());
}

}  // namespace internal
}  // namespace drive
