// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FILE_METADATA_UTIL_H_
#define IOS_CHROME_BROWSER_FILE_METADATA_UTIL_H_

#include "base/files/file_path.h"

namespace file_metadata_util {

// Synchronously sets a bit on the file system that prevents iCloud/iTunes
// backups of the file/folder at |path|. There must be a file/folder present at
// |path|.
void AddSkipSystemBackupAttributeToItem(const base::FilePath& path);

}  // namespace file_metadata_util

#endif  // IOS_CHROME_BROWSER_FILE_METADATA_UTIL_H_
