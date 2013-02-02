// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_OBSERVER_H_

#include "chrome/browser/chromeos/drive/drive_file_error.h"

namespace base {
class FilePath;
}

namespace drive {

// Interface for classes that need to observe events from classes implementing
// DriveFileSystemInterface.
// All events are notified on UI thread.
class DriveFileSystemObserver {
 public:
  // Triggered when a content of a directory has been changed.
  // |directory_path| is a virtual directory path (/drive/...) representing
  // changed directory.
  virtual void OnDirectoryChanged(const base::FilePath& directory_path) {
  }

  // Triggered when the file system is initially loaded.
  virtual void OnInitialLoadFinished(DriveFileError error) {
  }

  // Triggered when a document feed is fetched. |num_accumulated_entries|
  // tells the number of entries fetched so far.
  virtual void OnResourceListFetched(int num_accumulated_entries) {
  }

  // Triggered when the feed from the server is loaded.
  virtual void OnFeedFromServerLoaded() {
  }

  // Triggered when the file system is mounted.
  virtual void OnFileSystemMounted() {
  }

  // Triggered when the file system is being unmounted.
  virtual void OnFileSystemBeingUnmounted() {
  }

 protected:
  virtual ~DriveFileSystemObserver() {}
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_FILE_SYSTEM_OBSERVER_H_
