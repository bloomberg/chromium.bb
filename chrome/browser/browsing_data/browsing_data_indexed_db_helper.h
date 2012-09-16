// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace content {
class IndexedDBContext;
}

// BrowsingDataIndexedDBHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in indexed databases.  A
// client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataIndexedDBHelper
    : public base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper> {
 public:
  // Contains detailed information about an indexed database.
  struct IndexedDBInfo {
    IndexedDBInfo(
        const GURL& origin,
        int64 size,
        base::Time last_modified);
    ~IndexedDBInfo();

    GURL origin;
    int64 size;
    base::Time last_modified;
  };

  // Create a BrowsingDataIndexedDBHelper instance for the indexed databases
  // stored in |profile|'s user data directory.
  static BrowsingDataIndexedDBHelper* Create(
      content::IndexedDBContext* context);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const std::list<IndexedDBInfo>&)>&
          callback) = 0;
  // Requests a single indexed database to be deleted in the WEBKIT thread.
  virtual void DeleteIndexedDB(const GURL& origin) = 0;

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
  // Contains information about an indexed database.
  struct PendingIndexedDBInfo {
    PendingIndexedDBInfo(const GURL& origin, const string16& name);
    ~PendingIndexedDBInfo();

    bool operator<(const PendingIndexedDBInfo& other) const;

    GURL origin;
    string16 name;
  };

  CannedBrowsingDataIndexedDBHelper();

  // Return a copy of the IndexedDB helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataIndexedDBHelper* Clone();

  // Add a indexed database to the set of canned indexed databases that is
  // returned by this helper.
  void AddIndexedDB(const GURL& origin,
                    const string16& name);

  // Clear the list of canned indexed databases.
  void Reset();

  // True if no indexed databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored indexed databases.
  size_t GetIndexedDBCount() const;

  // Returns the current list of indexed data bases.
  const std::set<CannedBrowsingDataIndexedDBHelper::PendingIndexedDBInfo>&
      GetIndexedDBInfo() const;

  // BrowsingDataIndexedDBHelper methods.
  virtual void StartFetching(
      const base::Callback<void(const std::list<IndexedDBInfo>&)>&
          callback) OVERRIDE;

  virtual void DeleteIndexedDB(const GURL& origin) OVERRIDE {}

 private:
  virtual ~CannedBrowsingDataIndexedDBHelper();

  // Convert the pending indexed db info to indexed db info objects.
  void ConvertPendingInfoInWebKitThread();

  void NotifyInUIThread();

  // Lock to protect access to pending_indexed_db_info_;
  mutable base::Lock lock_;

  // Access to |pending_indexed_db_info_| is protected by |lock_| since it can
  // be accessed on the UI and on the WEBKIT thread.
  std::set<PendingIndexedDBInfo> pending_indexed_db_info_;

  // Access to |indexed_db_info_| is triggered indirectly via the UI thread and
  // guarded by |is_fetching_|. This means |indexed_db_info_| is only accessed
  // while |is_fetching_| is true. The flag |is_fetching_| is only accessed on
  // the UI thread.
  // In the context of this class |indexed_db_info_| is only accessed on the UI
  // thread.
  std::list<IndexedDBInfo> indexed_db_info_;

  // This only mutates on the UI thread.
  base::Callback<void(const std::list<IndexedDBInfo>&)> completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataIndexedDBHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_
