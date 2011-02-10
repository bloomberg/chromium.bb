// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_INDEXED_DB_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_INDEXED_DB_HELPER_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"

class Profile;

// BrowsingDataIndexedDBHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in indexed databases.  A
// client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.  The client must call CancelNotification() if it's
// destroyed before the callback is notified.
class BrowsingDataIndexedDBHelper
    : public base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper> {
 public:
  // Contains detailed information about an indexed database.
  struct IndexedDBInfo {
    IndexedDBInfo(
        const std::string& protocol,
        const std::string& host,
        unsigned short port,
        const std::string& database_identifier,
        const std::string& origin,
        const FilePath& file_path,
        int64 size,
        base::Time last_modified);
    ~IndexedDBInfo();

    bool IsFileSchemeData() {
      return protocol == chrome::kFileScheme;
    }

    std::string protocol;
    std::string host;
    unsigned short port;
    std::string database_identifier;
    std::string origin;
    FilePath file_path;
    int64 size;
    base::Time last_modified;
  };

  // Create a BrowsingDataIndexedDBHelper instance for the indexed databases
  // stored in |profile|'s user data directory.
  static BrowsingDataIndexedDBHelper* Create(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      Callback1<const std::vector<IndexedDBInfo>& >::Type* callback) = 0;
  // Cancels the notification callback (i.e., the window that created it no
  // longer exists).
  // This must be called only in the UI thread.
  virtual void CancelNotification() = 0;
  // Requests a single indexed database file to be deleted in the WEBKIT thread.
  virtual void DeleteIndexedDBFile(const FilePath& file_path) = 0;

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper>;
  virtual ~BrowsingDataIndexedDBHelper() {}
};

// This class is an implementation of BrowsingDataIndexedDBHelper that does
// not fetch its information from the indexed database tracker, but gets them
// passed as a parameter.
class CannedBrowsingDataIndexedDBHelper
    : public BrowsingDataIndexedDBHelper {
 public:
  explicit CannedBrowsingDataIndexedDBHelper(Profile* profile);

  // Add a indexed database to the set of canned indexed databases that is
  // returned by this helper.
  void AddIndexedDB(const GURL& origin,
                    const string16& description);

  // Clear the list of canned indexed databases.
  void Reset();

  // True if no indexed databases are currently stored.
  bool empty() const;

  // BrowsingDataIndexedDBHelper methods.
  virtual void StartFetching(
      Callback1<const std::vector<IndexedDBInfo>& >::Type* callback);
  virtual void CancelNotification() {}
  virtual void DeleteIndexedDBFile(const FilePath& file_path) {}

 private:
  struct PendingIndexedDBInfo {
    PendingIndexedDBInfo();
    PendingIndexedDBInfo(const GURL& origin, const string16& description);
    ~PendingIndexedDBInfo();

    GURL origin;
    string16 description;
  };

  virtual ~CannedBrowsingDataIndexedDBHelper();

  // Convert the pending indexed db info to indexed db info objects.
  void ConvertPendingInfoInWebKitThread();

  void NotifyInUIThread();

  Profile* profile_;

  // Lock to protect access to pending_indexed_db_info_;
  mutable base::Lock lock_;

  // This may mutate on WEBKIT and UI threads.
  std::vector<PendingIndexedDBInfo> pending_indexed_db_info_;

  // This only mutates on the WEBKIT thread.
  std::vector<IndexedDBInfo> indexed_db_info_;

  // This only mutates on the UI thread.
  scoped_ptr<Callback1<const std::vector<IndexedDBInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataIndexedDBHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_INDEXED_DB_HELPER_H_
