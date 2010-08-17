// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for Linux systems which don't have the inotify headers, and
// thus cannot build file_watcher_inotify.cc

#include "chrome/browser/file_watcher.h"

class FileWatcherImpl : public FileWatcher::PlatformDelegate {
 public:
  virtual bool Watch(const FilePath& path, FileWatcher::Delegate* delegate) {
    return false;
  }
};

FileWatcher::FileWatcher() {
  impl_ = new FileWatcherImpl();
}
