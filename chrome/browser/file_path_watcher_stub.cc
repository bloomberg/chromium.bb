// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for Unix systems which don't have the inotify headers, and
// thus cannot build file_watcher_inotify.cc

#include "chrome/browser/file_path_watcher.h"

class FilePathWatcherImpl : public FilePathWatcher::PlatformDelegate {
 public:
  virtual bool Watch(const FilePath& path, FileWatcher::Delegate* delegate) {
    return false;
  }
};

FilePathWatcher::FilePathWatcher() {
  impl_ = new FilePathWatcherImpl();
}
