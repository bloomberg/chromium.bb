// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a file for changes.

#ifndef BASE_FILE_WATCHER_H_
#define BASE_FILE_WATCHER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

class FilePath;
class MessageLoop;

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
  // OnFileChanged will be called back for each change to the file.
  // Any background operations will be ran on |backend_loop|, or inside Watch
  // if |backend_loop| is NULL.  Note: The directory containing |path| must
  // exist before you try to watch the file.
  // Returns false on error.
  bool Watch(const FilePath& path, Delegate* delegate,
             MessageLoop* backend_loop) {
    return impl_->Watch(path, delegate, backend_loop);
  }

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate : public base::RefCounted<PlatformDelegate> {
   public:
    virtual bool Watch(const FilePath& path, Delegate* delegate,
                       MessageLoop* backend_loop) = 0;

   protected:
    friend class base::RefCounted<PlatformDelegate>;

    virtual ~PlatformDelegate() {}
  };

 private:
  scoped_refptr<PlatformDelegate> impl_;

  DISALLOW_COPY_AND_ASSIGN(FileWatcher);
};

#endif  // BASE_FILE_WATCHER_H_
