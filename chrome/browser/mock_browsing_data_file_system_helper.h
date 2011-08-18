// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MOCK_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#define CHROME_BROWSER_MOCK_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#pragma once

#include <list>
#include <map>

#include "base/callback.h"
#include "chrome/browser/browsing_data_file_system_helper.h"
#include "webkit/fileapi/file_system_types.h"

// Mock for BrowsingDataFileSystemHelper.
// Use AddFileSystemSamples() or add directly to response_ list, then call
// Notify().
class MockBrowsingDataFileSystemHelper : public BrowsingDataFileSystemHelper {
 public:
  explicit MockBrowsingDataFileSystemHelper(Profile* profile);

  virtual void StartFetching(
      Callback1<const std::list<FileSystemInfo>& >::Type* callback);

  virtual void CancelNotification();

  virtual void DeleteFileSystemOrigin(const GURL& origin);

  // Adds a specific filesystem.
  void AddFileSystem(const GURL& origin,
                     bool has_persistent,
                     bool has_temporary);

  // Adds some FilesystemInfo samples.
  void AddFileSystemSamples();

  // Notifies the callback.
  void Notify();

  // Marks all filesystems as existing.
  void Reset();

  // Returns true if all filesystemss since the last Reset() invokation were
  // deleted.
  bool AllDeleted();

  GURL last_deleted_origin_;

 private:
  virtual ~MockBrowsingDataFileSystemHelper();

  Profile* profile_;

  scoped_ptr<Callback1<const std::list<FileSystemInfo>& >::Type >
      callback_;

  // Stores which filesystems exist.
  std::map<const std::string, bool> file_systems_;

  std::list<FileSystemInfo> response_;
};

#endif  // CHROME_BROWSER_MOCK_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
