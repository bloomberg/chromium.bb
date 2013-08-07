// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_write_watcher.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/logging.h"
#include "chrome/browser/google_apis/task_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace drive {
namespace internal {

namespace {
const int64 kWriteEventDelayInSeconds = 5;
}  // namespace

// base::FileWatcher needs to live in a thread that is allowed to do File IO
// and has a TYPE_IO message loop: that is, FILE thread. This class bridges the
// UI thread and FILE thread, and does all the main tasks in the FILE thread.
class FileWriteWatcher::FileWriteWatcherImpl {
 public:
  FileWriteWatcherImpl();

  // Forwards the call to DestoryOnFileThread(). This method must be used to
  // destruct the instance.
  void Destroy();

  // Forwards the call to StartWatchOnFileThread(). |callback| is called back
  // on the caller (UI) thread when the watch has started.
  // |on_write_event_callback| is called when a write has happened to the path.
  void StartWatch(const base::FilePath& path,
                  const StartWatchCallback& callback,
                  const base::Closure& on_write_event_callback);

  void set_delay(base::TimeDelta delay) { delay_ = delay; }

 private:
  ~FileWriteWatcherImpl();

  void DestroyOnFileThread();

  void StartWatchOnFileThread(const base::FilePath& path,
                              const StartWatchCallback& callback,
                              const base::Closure& on_write_event_callback);

  void OnWriteEvent(const base::FilePath& path, bool error);

  void InvokeCallback(const base::FilePath& path);

  struct PathWatchInfo {
    base::Closure on_write_event_callback;
    base::FilePathWatcher watcher;
    base::Timer timer;

    explicit PathWatchInfo(const base::Closure& callback)
        : on_write_event_callback(callback),
          timer(false /* retain_closure_on_reset */, false /* is_repeating */) {
    }
  };

  base::TimeDelta delay_;
  std::map<base::FilePath, PathWatchInfo*> watchers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FileWriteWatcherImpl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(FileWriteWatcherImpl);
};

FileWriteWatcher::FileWriteWatcherImpl::FileWriteWatcherImpl()
    : delay_(base::TimeDelta::FromSeconds(kWriteEventDelayInSeconds)),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileWriteWatcher::FileWriteWatcherImpl::Destroy() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Just forwarding the call to FILE thread.
  BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)->PostTask(
      FROM_HERE,
      base::Bind(&FileWriteWatcherImpl::DestroyOnFileThread,
                 base::Unretained(this)));
}

void FileWriteWatcher::FileWriteWatcherImpl::StartWatch(
    const base::FilePath& path,
    const StartWatchCallback& callback,
    const base::Closure& on_write_event_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Forwarding the call to FILE thread and relaying the |callback|.
  BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE)->PostTask(
      FROM_HERE,
      base::Bind(&FileWriteWatcherImpl::StartWatchOnFileThread,
                 base::Unretained(this),
                 path,
                 google_apis::CreateRelayCallback(callback),
                 google_apis::CreateRelayCallback(on_write_event_callback)));
}

FileWriteWatcher::FileWriteWatcherImpl::~FileWriteWatcherImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  STLDeleteContainerPairSecondPointers(watchers_.begin(), watchers_.end());
}

void FileWriteWatcher::FileWriteWatcherImpl::DestroyOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  delete this;
}

void FileWriteWatcher::FileWriteWatcherImpl::StartWatchOnFileThread(
    const base::FilePath& path,
    const StartWatchCallback& callback,
    const base::Closure& on_write_event_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  util::Log(logging::LOG_INFO, "Started watching modification to %s.",
            path.AsUTF8Unsafe().c_str());

  std::map<base::FilePath, PathWatchInfo*>::iterator it = watchers_.find(path);
  if (it != watchers_.end()) {
    // Do nothing if we are already watching the path.
    callback.Run(true);
    return;
  }

  // Start watching |path|.
  scoped_ptr<PathWatchInfo> info(new PathWatchInfo(on_write_event_callback));
  bool ok = info->watcher.Watch(
      path,
      false,  // recursive
      base::Bind(&FileWriteWatcherImpl::OnWriteEvent,
                 weak_ptr_factory_.GetWeakPtr()));
  watchers_[path] = info.release();
  callback.Run(ok);
}

void FileWriteWatcher::FileWriteWatcherImpl::OnWriteEvent(
    const base::FilePath& path,
    bool error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  util::Log(logging::LOG_INFO, "Detected modification to %s.",
            path.AsUTF8Unsafe().c_str());

  if (error)
    return;

  std::map<base::FilePath, PathWatchInfo*>::iterator it = watchers_.find(path);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  util::Log(logging::LOG_INFO, "Finished watching modification to %s.",
            path.AsUTF8Unsafe().c_str());

  std::map<base::FilePath, PathWatchInfo*>::iterator it = watchers_.find(path);
  DCHECK(it != watchers_.end());

  base::Closure callback = it->second->on_write_event_callback;
  delete it->second;
  watchers_.erase(it);

  callback.Run();
}

FileWriteWatcher::FileWriteWatcher(file_system::OperationObserver* observer)
    : watcher_impl_(new FileWriteWatcherImpl),
      operation_observer_(observer),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FileWriteWatcher::~FileWriteWatcher() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FileWriteWatcher::StartWatch(const base::FilePath& file_path,
                                  const std::string& resource_id,
                                  const StartWatchCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  watcher_impl_->StartWatch(file_path,
                            callback,
                            base::Bind(&FileWriteWatcher::OnWriteEvent,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       resource_id));
}

void FileWriteWatcher::DisableDelayForTesting() {
  watcher_impl_->set_delay(base::TimeDelta());
}

void FileWriteWatcher::OnWriteEvent(const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  operation_observer_->OnCacheFileUploadNeededByOperation(resource_id);
}

}  // namespace internal
}  // namespace drive
