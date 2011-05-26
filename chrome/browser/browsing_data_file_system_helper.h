// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback_old.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_types.h"

class Profile;

// BrowsingDataFileSystemHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in local filesystems.  A
// client of this class needs to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.  The client must call CancelNotification() if it's
// destroyed before the callback is notified.
class BrowsingDataFileSystemHelper
    : public base::RefCountedThreadSafe<BrowsingDataFileSystemHelper> {
 public:
  // Contains detailed information about a filesystem
  struct FileSystemInfo {
    FileSystemInfo(
        const GURL& origin,
        bool has_persistent,
        bool has_temporary,
        int64 usage_persistent,
        int64 usage_temporary);
    ~FileSystemInfo();

    bool IsFileSchemeData() {
      return origin.scheme() == chrome::kFileScheme;
    }

    GURL origin;
    bool has_persistent;
    bool has_temporary;
    int64 usage_persistent;
    int64 usage_temporary;
  };

  // Create a BrowsingDataFileSystemHelper instance for the filesystems
  // stored in |profile|'s user data directory.
  static BrowsingDataFileSystemHelper* Create(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      Callback1<const std::vector<FileSystemInfo>& >::Type* callback) = 0;
  // Cancels the notification callback (i.e., the window that created it no
  // longer exists).
  // This must be called only in the UI thread.
  virtual void CancelNotification() = 0;
  // Requests a single filesystem to be deleted in the FILE thread.
  virtual void DeleteFileSystemOrigin(const GURL& origin) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataFileSystemHelper>;
  virtual ~BrowsingDataFileSystemHelper() {}
};

// This class is an implementation of BrowsingDataFileSystemHelper that does
// not fetch its information from the filesystem tracker, but gets it passed
// in as a parameter.
class CannedBrowsingDataFileSystemHelper
    : public BrowsingDataFileSystemHelper {
 public:
  explicit CannedBrowsingDataFileSystemHelper(Profile* profile);

  // Return a copy of the filesystem helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataFileSystemHelper* Clone();

  // Add a filesystem to the set of canned filesystems that is
  // returned by this helper.
  void AddFileSystem(const GURL& origin,
                     fileapi::FileSystemType type,
                     int64 size);

  // Clear the list of canned filesystems.
  void Reset();

  // True if no filesystems are currently stored.
  bool empty() const;

  // BrowsingDataFileSystemHelper methods.
  virtual void StartFetching(
      Callback1<const std::vector<FileSystemInfo>& >::Type* callback);
  virtual void CancelNotification() {}
  virtual void DeleteFileSystemOrigin(const GURL& origin) {}

 private:
  struct PendingFileSystemInfo {
    PendingFileSystemInfo();
    PendingFileSystemInfo(const GURL& origin,
                          fileapi::FileSystemType type,
                          int64 size);
    ~PendingFileSystemInfo();

    GURL origin;
    fileapi::FileSystemType type;
    int64 size;
  };

  // StartFetching's callback should be executed asynchronously, Notify handles
  // that nicely.
  void Notify();

  virtual ~CannedBrowsingDataFileSystemHelper();

  Profile* profile_;

  std::vector<PendingFileSystemInfo> pending_file_system_info_;

  std::vector<FileSystemInfo> file_system_info_;

  scoped_ptr<Callback1<const std::vector<FileSystemInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataFileSystemHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
