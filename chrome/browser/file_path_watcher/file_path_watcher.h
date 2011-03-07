// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a file or directory for changes.

#ifndef CHROME_BROWSER_FILE_PATH_WATCHER_FILE_PATH_WATCHER_H_
#define CHROME_BROWSER_FILE_PATH_WATCHER_FILE_PATH_WATCHER_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "content/browser/browser_thread.h"

// This class lets you register interest in changes on a FilePath.
// The delegate will get called whenever the file or directory referenced by the
// FilePath is changed, including created or deleted. Due to limitations in the
// underlying OS APIs, spurious notifications might occur that don't relate to
// an actual change to the watch target.
class FilePathWatcher {
 public:
  // Declares the callback client code implements to receive notifications. Note
  // that implementations of this interface should not keep a reference to the
  // corresponding FileWatcher object to prevent a reference cycle.
  class Delegate : public base::RefCountedThreadSafe<Delegate> {
   public:
    virtual ~Delegate() {}
    virtual void OnFilePathChanged(const FilePath& path) = 0;
    // Called when platform specific code detected an error. The watcher will
    // not call OnFilePathChanged for future changes.
    virtual void OnError() {}
  };

  FilePathWatcher();
  ~FilePathWatcher();

  // Register interest in any changes on |path|. OnPathChanged will be called
  // back for each change. Returns true on success.
  bool Watch(const FilePath& path, Delegate* delegate) WARN_UNUSED_RESULT;

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate
      : public base::RefCountedThreadSafe<PlatformDelegate,
                                          BrowserThread::DeleteOnFileThread> {
   public:
    // Start watching for the given |path| and notify |delegate| about changes.
    virtual bool Watch(const FilePath& path, Delegate* delegate)
        WARN_UNUSED_RESULT = 0;

    // Stop watching. This is called from FilePathWatcher's dtor in order to
    // allow to shut down properly while the object is still alive.
    virtual void Cancel() {}

   protected:
    friend struct BrowserThread::DeleteOnThread<BrowserThread::FILE>;
    friend class DeleteTask<PlatformDelegate>;

    virtual ~PlatformDelegate() {}
  };

 private:
  scoped_refptr<PlatformDelegate> impl_;

  DISALLOW_COPY_AND_ASSIGN(FilePathWatcher);
};

#endif  // CHROME_BROWSER_FILE_PATH_WATCHER_FILE_PATH_WATCHER_H_
