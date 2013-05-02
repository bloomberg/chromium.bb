// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OBSERVER_H_

#include "chrome/browser/chromeos/drive/file_errors.h"

namespace base {
class FilePath;
}

namespace drive {

// Interface for classes that need to observe events from classes implementing
// FileSystemInterface.
// All events are notified on UI thread.
class FileSystemObserver {
 public:
  // Triggered when a content of a directory has been changed.
  // |directory_path| is a virtual directory path (/drive/...) representing
  // changed directory.
  virtual void OnDirectoryChanged(const base::FilePath& directory_path) {
  }

  // Triggered when the file system is initially loaded.
  virtual void OnInitialLoadFinished() {
  }

  // Triggered when the feed from the server is loaded.
  virtual void OnFeedFromServerLoaded() {
  }

 protected:
  virtual ~FileSystemObserver() {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILE_SYSTEM_OBSERVER_H_
