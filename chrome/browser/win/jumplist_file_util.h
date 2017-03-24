// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_
#define CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_

#include "base/files/file_path.h"

// Maximum number of icon files allowed to delete per jumplist update.
const int kFileDeleteLimit = 100;

// Folder delete status enumeration, used in Delete* methods below.
// This is used for UMA. Do not delete entries, and keep in sync with
// histograms.xml.
enum FolderDeleteResult {
  SUCCEED = 0,
  // File name's length exceeds MAX_PATH. This shouldn't happen.
  FAIL_INVALID_FILE_PATH,
  // JumpListIcons{,Old} directories are read-only. This may heppen.
  FAIL_READ_ONLY_DIRECTORY,
  // Since JumpListIcons{,Old} are directories. This shouldn't happen.
  FAIL_DELETE_SINGLE_FILE,
  // JumpListIcons{,Old} should not have sub-directories, so this shouldn't
  // happen. If this happens, the root cause must be found.
  FAIL_SUBDIRECTORY_EXISTS,
  // Delete maximum files allowed succeeds. However, in the process of deleting
  // these files, it fails to delete some other files. This may happen.
  FAIL_DELETE_MAX_FILES_WITH_ERRORS,
  // Fail to delete maximum files allowed when the maximum attempt failures
  // are hit. This may heppen.
  FAIL_MAX_DELETE_FAILURES,
  // Fail to remove the raw empty directory. This may happen.
  FAIL_REMOVE_RAW_DIRECTORY,
  // Add new items before this one, always keep this one at the end.
  END
};

// An enumeration indicating if a directory is empty or not.
// This is used for UMA. Do not delete entries, and keep in sync with
// histograms.xml.
enum DirectoryEmptyStatus {
  EMPTY = 0,
  NON_EMPTY,
  // Add new items before this one, always keep this one at the end.
  EMPTY_STATUS_END
};

// This method is similar to base::DeleteFileRecursive in
// file_util_win.cc with the following differences.
// 1) It has an input parameter |max_file_deleted| to specify the maximum files
//    allowed to delete as well as the maximum attempt failures allowd per run.
// 2) It deletes only the files in |path|. All subdirectories in |path| are
//    untouched but are considered as attempt failures.
// 3) Detailed delete status is returned.
FolderDeleteResult DeleteFiles(const base::FilePath& path,
                               const base::FilePath::StringType& pattern,
                               int max_file_deleted);

// This method is similar to base::DeleteFile in file_util_win.cc
// with the following differences.
// 1) It has an input parameter |max_file_deleted| to specify the maximum files
//    allowed to delete as well as the maximum attempt failures allowd per run.
// 2) It deletes only the files in |path|. All subdirectories in |path| are
//    untouched but are considered as attempt failures.
// 3) |path| won't be removed even if all its contents are deleted successfully.
// 4) Detailed delete status is returned.
FolderDeleteResult DeleteDirectoryContent(const base::FilePath& path,
                                          int max_file_deleted);

// This method firstly calls DeleteDirectoryContent() to delete the contents in
// |path|. If |path| is empty after the call, it is removed.
FolderDeleteResult DeleteDirectory(const base::FilePath& path,
                                   int max_file_deleted);

// Deletes the directory at |path| and records the result to UMA.
void DeleteDirectoryAndLogResults(const base::FilePath& path,
                                  int max_file_deleted);

#endif  // CHROME_BROWSER_WIN_JUMPLIST_FILE_UTIL_H_
