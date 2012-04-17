// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_FILE_PATH_WATCHER_WRAPPER_H_
#define NET_DNS_FILE_PATH_WATCHER_WRAPPER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_export.h"

namespace base {
namespace files {
class FilePathWatcher;
}
}

namespace net {

// Convenience wrapper which holds a FilePathWatcher and a
// FilePathWatcher::Delegate which forwards its calls to a Callback.
class NET_EXPORT FilePathWatcherWrapper
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  FilePathWatcherWrapper();
  // When deleted, automatically cancels the FilePathWatcher.
  virtual ~FilePathWatcherWrapper();

  // Starts watching the file at |path|. Returns true if Watch succeeds.
  // If so, |callback| will be called with true on change and false on error.
  // After failure the watch is cancelled and will have to be restarted.
  bool Watch(const FilePath& path,
             const base::Callback<void(bool succeeded)>& callback);

  bool IsWatching() const;

  // Cancels the current watch.
  void Cancel();

 protected:
  // Creates a FilePathWatcher. Override to provide a different implementation.
  virtual scoped_ptr<base::files::FilePathWatcher> CreateFilePathWatcher();

 private:
  class WatcherDelegate;

  void OnFilePathChanged();
  void OnFilePathError();

  base::Callback<void(bool succeeded)> callback_;
  scoped_ptr<base::files::FilePathWatcher> watcher_;
  scoped_refptr<WatcherDelegate> delegate_;
  DISALLOW_COPY_AND_ASSIGN(FilePathWatcherWrapper);
};

}  // namespace net

#endif  // NET_DNS_FILE_PATH_WATCHER_WRAPPER_H_

