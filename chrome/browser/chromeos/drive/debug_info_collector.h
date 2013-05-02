// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DEBUG_INFO_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DEBUG_INFO_COLLECTOR_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"

namespace drive {

// This class provides some methods which are useful to show the debug
// info on chrome://drive-internals page.
// All the method should be called on UI thread.
class DebugInfoCollector {
 public:
  DebugInfoCollector(FileSystemInterface* file_system,
                     FileCache* file_cache);
  ~DebugInfoCollector();

  // Iterates all files in the file cache and calls |iteration_callback| for
  // each file. |completion_callback| is run upon completion.
  // |iteration_callback| and |completion_callback| must not be null.
  void IterateFileCache(const CacheIterateCallback& iteration_callback,
                        const base::Closure& completion_callback);

  // Returns miscellaneous metadata of the file system like the largest
  // timestamp. |callback| must not be null.
  void GetMetadata(const GetFilesystemMetadataCallback& callback);

 private:
  FileSystemInterface* file_system_;  // Not owned.
  FileCache* file_cache_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(DebugInfoCollector);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DEBUG_INFO_COLLECTOR_H_
