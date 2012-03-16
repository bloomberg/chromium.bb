// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/file_path_watcher_wrapper.h"

#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/logging.h"

namespace net {

// A FilePathWatcher::Delegate that forwards calls to FilePathWatcherCallback.
class FilePathWatcherWrapper::WatcherDelegate
    : public base::files::FilePathWatcher::Delegate {
 public:
  explicit WatcherDelegate(FilePathWatcherWrapper* watcher)
      : watcher_(watcher) {}

  void Cancel() {
    watcher_ = NULL;
  }

  void OnFilePathChanged(const FilePath& path) OVERRIDE {
    if (watcher_)
      watcher_->OnFilePathChanged();
  }

  void OnFilePathError(const FilePath& path) OVERRIDE {
    if (watcher_)
      watcher_->OnFilePathError();
  }

 private:
  virtual ~WatcherDelegate() {}
  FilePathWatcherWrapper* watcher_;
};

FilePathWatcherWrapper::FilePathWatcherWrapper() {
  // No need to restrict the thread until the state is created in Watch.
  DetachFromThread();
}

FilePathWatcherWrapper::~FilePathWatcherWrapper() {
  Cancel();
}

void FilePathWatcherWrapper::Cancel() {
  DCHECK(CalledOnValidThread());
  if (delegate_) {
    delegate_->Cancel();
    delegate_.release();
  }
  watcher_.reset(NULL);
  // All state is gone so it's okay to detach from thread.
  DetachFromThread();
}

bool FilePathWatcherWrapper::Watch(
    const FilePath& path,
    const base::Callback<void(bool succeeded)>& callback) {
  DCHECK(CalledOnValidThread());
  if (delegate_)
    delegate_->Cancel();
  callback_ = callback;
  watcher_ = this->CreateFilePathWatcher();
  delegate_ = new WatcherDelegate(this);
  if (watcher_->Watch(path, delegate_.get()))
    return true;

  Cancel();
  return false;
}

bool FilePathWatcherWrapper::IsWatching() const {
  DCHECK(CalledOnValidThread());
  return watcher_.get() != NULL;
}

scoped_ptr<base::files::FilePathWatcher>
    FilePathWatcherWrapper::CreateFilePathWatcher() {
  return scoped_ptr<base::files::FilePathWatcher>(
      new base::files::FilePathWatcher());
}

void FilePathWatcherWrapper::OnFilePathChanged() {
  DCHECK(CalledOnValidThread());
  callback_.Run(true);
}

void FilePathWatcherWrapper::OnFilePathError() {
  DCHECK(CalledOnValidThread());
  base::Callback<void(bool succeeded)> callback = callback_;
  Cancel();
  callback.Run(false);
}

}  // namespace net

