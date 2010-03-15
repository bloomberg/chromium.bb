// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a file for changes.

#ifndef CHROME_BROWSER_FILE_WATCHER_H_
#define CHROME_BROWSER_FILE_WATCHER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"

class FilePath;
// This class lets you register interest in changes on a file. The delegate
// will get called whenever the file is changed, including created or deleted.
// WARNING: To be able to get create/delete notifications and to work cross
// platform, we actually listen for changes to the directory containing
// the file.
// WARNING: On OSX and Windows, the OS API doesn't tell us which file in the
// directory changed.  We work around this by watching the file time, but this
// can result in some extra notifications if we get other notifications within
// 2s of the file having changed.
class FileWatcher {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnFileChanged(const FilePath& path) = 0;
  };

  FileWatcher();
  ~FileWatcher() {}

  // Register interest in any changes on the file |path|.
  // OnFileChanged will be called back for each change to the file.  Any
  // background operations will be ran on |backend_thread_id|.  Note: The
  // directory containing |path| must exist before you try to watch the file.
  // Returns false if the watch can't be added.
  bool Watch(const FilePath& path, Delegate* delegate) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::FILE));
    return impl_->Watch(path, delegate);
  }

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate
      : public base::RefCountedThreadSafe<PlatformDelegate,
                                          ChromeThread::DeleteOnFileThread> {
   public:
    virtual ~PlatformDelegate() {}

    virtual bool Watch(const FilePath& path, Delegate* delegate) = 0;
  };

 private:
  scoped_refptr<PlatformDelegate> impl_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcher);
};

#endif  // CHROME_BROWSER_FILE_WATCHER_H_
