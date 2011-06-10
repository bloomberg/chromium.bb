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

// Defines an interface for classes that deal with aggregating and deleting
// browsing data stored in an origin's file systems. Clients of this interface
// must call StartFetching from the UI thread to initiate the flow, and will
// be notified via a callback at some later point (also in the UI thread). If
// the client is destroyed before the callback is triggered, it must call
// CancelNotification.
class BrowsingDataFileSystemHelper
    : public base::RefCountedThreadSafe<BrowsingDataFileSystemHelper> {
 public:
  // Detailed information about a file system, including it's origin GURL,
  // the presence or absence of persistent and temporary storage, and the
  // amount of data (in bytes) each contains.
  struct FileSystemInfo {
    FileSystemInfo(
        const GURL& origin,
        bool has_persistent,
        bool has_temporary,
        int64 usage_persistent,
        int64 usage_temporary);
    ~FileSystemInfo();

    GURL origin;
    bool has_persistent;
    bool has_temporary;
    int64 usage_persistent;
    int64 usage_temporary;
  };

  // Create a BrowsingDataFileSystemHelper instance for the file systems
  // stored in |profile|'s user data directory.
  static BrowsingDataFileSystemHelper* Create(Profile* profile);

  // Starts the process of fetching file system data, which will call |callback|
  // upon completion, passing it a constant vector of FileSystemInfo objects.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      Callback1<const std::vector<FileSystemInfo>& >::Type* callback) = 0;

  // Cancels the notification callback associated with StartFetching. Clients
  // that are destroyed before the callback is triggered must call this, and
  // it must be called only on the UI thread.
  virtual void CancelNotification() = 0;

  // Deletes all file systems associated with |origin|. The deletion will occur
  // on the FILE thread, but this function must be called only on the UI thread.
  virtual void DeleteFileSystemOrigin(const GURL& origin) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataFileSystemHelper>;
  virtual ~BrowsingDataFileSystemHelper() {}
};

// An implementation of the BrowsingDataFileSystemHelper interface that can
// be manually populated with data, rather than fetching data from the file
// systems created in a particular Profile.
class CannedBrowsingDataFileSystemHelper
    : public BrowsingDataFileSystemHelper {
 public:
  // |profile| is unused in this canned implementation, but it's the interface
  // we're writing to, so we'll accept it, but not store it.
  explicit CannedBrowsingDataFileSystemHelper(Profile* profile);

  // Creates a copy of the file system helper. StartFetching can only respond
  // to one client at a time; we need to be able to act on multiple parallel
  // requests in certain situations (see CookiesTreeModel and its clients). For
  // these cases, simply clone the object and fire off another fetching process.
  CannedBrowsingDataFileSystemHelper* Clone();

  // Manually add a filesystem to the set of canned file systems that this
  // helper returns via StartFetching. If an origin contains both a temporary
  // and a persistent filesystem, AddFileSystem must be called twice (once for
  // each file system type).
  void AddFileSystem(const GURL& origin,
                     fileapi::FileSystemType type,
                     int64 size);

  // Clear this helper's list of canned filesystems.
  void Reset();

  // True if no filesystems are currently stored.
  bool empty() const;

  // BrowsingDataFileSystemHelper methods.
  virtual void StartFetching(
      Callback1<const std::vector<FileSystemInfo>& >::Type* callback);
  virtual void CancelNotification();
  virtual void DeleteFileSystemOrigin(const GURL& origin) {}

 private:
  // Used by Clone() to create an object without a Profile
  CannedBrowsingDataFileSystemHelper();
  virtual ~CannedBrowsingDataFileSystemHelper();

  // AddFileSystem doesn't store file systems directly, but holds them until
  // StartFetching is called. At that point, the pending file system data is
  // merged with the current file system data before being returned to the
  // client.
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

  // Triggers the success callback as the end of a StartFetching workflow. This
  // must be called on the UI thread.
  void NotifyOnUIThread();

  // Holds file systems that have been added to the helper until StartFetching
  // is called.
  std::vector<PendingFileSystemInfo> pending_file_system_info_;

  // Holds the current list of file systems returned to the client after
  // StartFetching is called.
  std::vector<FileSystemInfo> file_system_info_;

  // Holds the callback passed in at the beginning of the StartFetching workflow
  // so that it can be triggered via NotifyOnUIThread.
  scoped_ptr<Callback1<const std::vector<FileSystemInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information: set to true
  // when StartFetching is called on the UI thread, and reset to false when
  // NotifyOnUIThread triggers the success callback.
  // This property only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataFileSystemHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_FILE_SYSTEM_HELPER_H_
