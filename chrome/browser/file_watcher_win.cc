// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_watcher.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/ref_counted.h"
#include "base/time.h"

namespace {

class FileWatcherImpl : public FileWatcher::PlatformDelegate,
                        public base::ObjectWatcher::Delegate {
 public:
  FileWatcherImpl() : delegate_(NULL), handle_(INVALID_HANDLE_VALUE) {}

  virtual bool Watch(const FilePath& path, FileWatcher::Delegate* delegate);

  // Callback from MessageLoopForIO.
  virtual void OnObjectSignaled(HANDLE object);

 private:
  virtual ~FileWatcherImpl();

  // Delegate to notify upon changes.
  FileWatcher::Delegate* delegate_;

  // Path we're watching (passed to delegate).
  FilePath path_;

  // Handle for FindFirstChangeNotification.
  HANDLE handle_;

  // ObjectWatcher to watch handle_ for events.
  base::ObjectWatcher watcher_;

  // Keep track of the last modified time of the file.  We use nulltime
  // to represent the file not existing.
  base::Time last_modified_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcherImpl);
};

FileWatcherImpl::~FileWatcherImpl() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    watcher_.StopWatching();
    FindCloseChangeNotification(handle_);
  }
}

bool FileWatcherImpl::Watch(const FilePath& path,
                            FileWatcher::Delegate* delegate) {
  DCHECK(path_.value().empty());  // Can only watch one path.
  file_util::FileInfo file_info;
  if (file_util::GetFileInfo(path, &file_info))
    last_modified_ = file_info.last_modified;

  // FindFirstChangeNotification watches directories, so use the parent path.
  handle_ = FindFirstChangeNotification(
      path.DirName().value().c_str(),
      false,  // Don't watch subtrees
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
      FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_DIR_NAME);
  if (handle_ == INVALID_HANDLE_VALUE)
    return false;

  delegate_ = delegate;
  path_ = path;
  watcher_.StartWatching(handle_, this);

  return true;
}

void FileWatcherImpl::OnObjectSignaled(HANDLE object) {
  DCHECK(object == handle_);
  // Make sure we stay alive through the body of this function.
  scoped_refptr<FileWatcherImpl> keep_alive(this);

  file_util::FileInfo file_info;
  bool file_exists = file_util::GetFileInfo(path_, &file_info);
  if (file_exists && (last_modified_.is_null() ||
      last_modified_ != file_info.last_modified)) {
    last_modified_ = file_info.last_modified;
    delegate_->OnFileChanged(path_);
  } else if (file_exists && (base::Time::Now() - last_modified_ <
             base::TimeDelta::FromSeconds(2))) {
    // Since we only have a resolution of 1s, if we get a callback within
    // 2s of the file having changed, go ahead and notify our observer.  This
    // might be from a different file change, but it's better to notify too
    // much rather than miss a notification.
    delegate_->OnFileChanged(path_);
  } else if (!file_exists && !last_modified_.is_null()) {
    last_modified_ = base::Time();
    delegate_->OnFileChanged(path_);
  }

  // Register for more notifications on file change.
  BOOL ok = FindNextChangeNotification(object);
  DCHECK(ok);
  watcher_.StartWatching(object, this);
}

}  // namespace

FileWatcher::FileWatcher() {
  impl_ = new FileWatcherImpl();
}
