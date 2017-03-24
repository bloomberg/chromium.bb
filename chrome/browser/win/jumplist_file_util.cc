// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/jumplist_file_util.h"

#include <Shlwapi.h>
#include <windows.h>

#include "base/files/file_enumerator.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"

FolderDeleteResult DeleteFiles(const base::FilePath& path,
                               const base::FilePath::StringType& pattern,
                               int max_file_deleted) {
  int success_count = 0;
  int failure_count = 0;
  FolderDeleteResult delete_status = SUCCEED;

  base::FileEnumerator traversal(
      path, false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES, pattern);
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Try to clear the read-only bit if we find it.
    base::FileEnumerator::FileInfo info = traversal.GetInfo();
    if (info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) {
      SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~FILE_ATTRIBUTE_READONLY);
    }

    if (info.IsDirectory()) {
      // JumpListIcons{,Old} directories shouldn't have sub-directories.
      // If any of them does for unknown reasons, don't delete them. Instead,
      // increment the failure count and record this information.
      delete_status = FAIL_SUBDIRECTORY_EXISTS;
      failure_count++;
    } else if (!::DeleteFile(current.value().c_str())) {
      failure_count++;
    } else {
      success_count++;
    }
    // If it deletes max_file_deleted files with any attempt failures, record
    // this information in |delete_status|.
    if (success_count >= max_file_deleted) {
      // The desired max number of files have been deleted.
      return failure_count ? FAIL_DELETE_MAX_FILES_WITH_ERRORS : delete_status;
    }
    if (failure_count >= max_file_deleted) {
      // The desired max number of failures have been hit.
      return FAIL_MAX_DELETE_FAILURES;
    }
  }
  return delete_status;
}

FolderDeleteResult DeleteDirectoryContent(const base::FilePath& path,
                                          int max_file_deleted) {
  base::ThreadRestrictions::AssertIOAllowed();

  if (path.empty())
    return SUCCEED;

  // For JumpListIcons{,Old} directories, since their names are shorter than
  // MAX_PATH, hitting the code in the if-block below is unexpected.
  if (path.value().length() >= MAX_PATH)
    return FAIL_INVALID_FILE_PATH;

  DWORD attr = GetFileAttributes(path.value().c_str());
  // We're done if we can't find the path.
  if (attr == INVALID_FILE_ATTRIBUTES)
    return SUCCEED;
  // Try to clear the read-only bit if we find it.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !SetFileAttributes(path.value().c_str(),
                         attr & ~FILE_ATTRIBUTE_READONLY)) {
    return FAIL_READ_ONLY_DIRECTORY;
  }

  // If |path| is a file, simply delete it. However, since JumpListIcons{,Old}
  // are directories, hitting the code inside the if-block below is unexpected.
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
    ::DeleteFile(path.value().c_str());
    return FAIL_DELETE_SINGLE_FILE;
  }

  // If |path| is a directory, delete at most |max_file_deleted| files in it.
  return DeleteFiles(path, L"*", max_file_deleted);
}

FolderDeleteResult DeleteDirectory(const base::FilePath& path,
                                   int max_file_deleted) {
  base::ThreadRestrictions::AssertIOAllowed();
  // Delete at most |max_file_deleted| files in |path|.
  FolderDeleteResult delete_status =
      DeleteDirectoryContent(path, max_file_deleted);
  // Since DeleteDirectoryContent() can only delete at most |max_file_deleted|
  // files, its return value cannot indicate if |path| is empty or not.
  // Instead, use PathIsDirectoryEmpty to check if |path| is empty and remove it
  // if it is.
  if (::PathIsDirectoryEmpty(path.value().c_str()) &&
      !::RemoveDirectory(path.value().c_str())) {
    delete_status = FAIL_REMOVE_RAW_DIRECTORY;
  }
  return delete_status;
}

void DeleteDirectoryAndLogResults(const base::FilePath& path,
                                  int max_file_deleted) {
  FolderDeleteResult delete_status = DeleteDirectory(path, max_file_deleted);
  UMA_HISTOGRAM_ENUMERATION("WinJumplist.DeleteStatusJumpListIconsOld",
                            delete_status, END);
}
