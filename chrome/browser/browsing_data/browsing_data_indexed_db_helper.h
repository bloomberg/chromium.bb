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
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/public/browser/indexed_db_context.h"
#include "url/gurl.h"

class Profile;

// BrowsingDataIndexedDBHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in indexed databases.  A
// client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class BrowsingDataIndexedDBHelper
    : public base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper> {
 public:
  // Create a BrowsingDataIndexedDBHelper instance for the indexed databases
  // stored in |profile|'s user data directory.
  explicit BrowsingDataIndexedDBHelper(content::IndexedDBContext* content);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const std::list<content::IndexedDBInfo>&)>&
          callback);
  // Requests a single indexed database to be deleted in the IndexedDB thread.
  virtual void DeleteIndexedDB(const GURL& origin);

 protected:
  virtual ~BrowsingDataIndexedDBHelper();

  scoped_refptr<content::IndexedDBContext> indexed_db_context_;

  // Access to |indexed_db_info_| is triggered indirectly via the UI thread and
  // guarded by |is_fetching_|. This means |indexed_db_info_| is only accessed
  // while |is_fetching_| is true. The flag |is_fetching_| is only accessed on
  // the UI thread.
  // In the context of this class |indexed_db_info_| is only accessed on the
  // context's IndexedDB thread.
  std::list<content::IndexedDBInfo> indexed_db_info_;

  // This only mutates on the UI thread.
  base::Callback<void(const std::list<content::IndexedDBInfo>&)>
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notified the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

 private:
  friend class base::RefCountedThreadSafe<BrowsingDataIndexedDBHelper>;

  // Enumerates all indexed database files in the IndexedDB thread.
  void FetchIndexedDBInfoInIndexedDBThread();
  // Notifies the completion callback in the UI thread.
  void NotifyInUIThread();
  // Delete a single indexed database in the IndexedDB thread.
  void DeleteIndexedDBInIndexedDBThread(const GURL& origin);

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataIndexedDBHelper);
};

// This class is an implementation of BrowsingDataIndexedDBHelper that does
// not fetch its information from the indexed database tracker, but gets them
// passed as a parameter.
class CannedBrowsingDataIndexedDBHelper
    : public BrowsingDataIndexedDBHelper {
 public:
  // Contains information about an indexed database.
  struct PendingIndexedDBInfo {
    PendingIndexedDBInfo(const GURL& origin, const base::string16& name);
    ~PendingIndexedDBInfo();

    bool operator<(const PendingIndexedDBInfo& other) const;

    GURL origin;
    base::string16 name;
  };

  explicit CannedBrowsingDataIndexedDBHelper(
      content::IndexedDBContext* context);

  // Return a copy of the IndexedDB helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // every time we instantiate a cookies tree model for it.
  CannedBrowsingDataIndexedDBHelper* Clone();

  // Add a indexed database to the set of canned indexed databases that is
  // returned by this helper.
  void AddIndexedDB(const GURL& origin,
                    const base::string16& name);

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
      const base::Callback<void(const std::list<content::IndexedDBInfo>&)>&
          callback) OVERRIDE;
  virtual void DeleteIndexedDB(const GURL& origin) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataIndexedDBHelper();

  std::set<PendingIndexedDBInfo> pending_indexed_db_info_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataIndexedDBHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_INDEXED_DB_HELPER_H_
