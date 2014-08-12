// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_

#include <list>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/common/url_constants.h"
#include "url/gurl.h"
#include "webkit/browser/database/database_tracker.h"
#include "webkit/common/database/database_identifier.h"

class Profile;

// This class fetches database information in the FILE thread, and notifies
// the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class BrowsingDataDatabaseHelper
    : public base::RefCountedThreadSafe<BrowsingDataDatabaseHelper> {
 public:
  // Contains detailed information about a web database.
  struct DatabaseInfo {
    DatabaseInfo(const webkit_database::DatabaseIdentifier& identifier,
                 const std::string& database_name,
                 const std::string& description,
                 int64 size,
                 base::Time last_modified);
    ~DatabaseInfo();

    webkit_database::DatabaseIdentifier identifier;
    std::string database_name;
    std::string description;
    int64 size;
    base::Time last_modified;
  };

  explicit BrowsingDataDatabaseHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      const base::Callback<void(const std::list<DatabaseInfo>&)>& callback);

  // Requests a single database to be deleted in the FILE thread. This must be
  // called in the UI thread.
  virtual void DeleteDatabase(const std::string& origin_identifier,
                              const std::string& name);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataDatabaseHelper>;
  virtual ~BrowsingDataDatabaseHelper();

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread();

  // Access to |database_info_| is triggered indirectly via the UI thread and
  // guarded by |is_fetching_|. This means |database_info_| is only accessed
  // while |is_fetching_| is true. The flag |is_fetching_| is only accessed on
  // the UI thread.
  // In the context of this class |database_info_| is only accessed on the FILE
  // thread.
  std::list<DatabaseInfo> database_info_;

  // This member is only mutated on the UI thread.
  base::Callback<void(const std::list<DatabaseInfo>&)> completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This member is only mutated on the UI thread.
  bool is_fetching_;

 private:
  // Enumerates all databases. This must be called in the FILE thread.
  void FetchDatabaseInfoOnFileThread();

  // Delete a single database file. This must be called in the FILE thread.
  void DeleteDatabaseOnFileThread(const std::string& origin,
                                  const std::string& name);

  scoped_refptr<webkit_database::DatabaseTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataDatabaseHelper);
};

// This class is a thin wrapper around BrowsingDataDatabaseHelper that does not
// fetch its information from the database tracker, but gets them passed as
// a parameter during construction.
class CannedBrowsingDataDatabaseHelper : public BrowsingDataDatabaseHelper {
 public:
  struct PendingDatabaseInfo {
    PendingDatabaseInfo(const GURL& origin,
                        const std::string& name,
                        const std::string& description);
    ~PendingDatabaseInfo();

    // The operator is needed to store |PendingDatabaseInfo| objects in a set.
    bool operator<(const PendingDatabaseInfo& other) const;

    GURL origin;
    std::string name;
    std::string description;
  };

  explicit CannedBrowsingDataDatabaseHelper(Profile* profile);

  // Return a copy of the database helper. Only one consumer can use the
  // StartFetching method at a time, so we need to create a copy of the helper
  // everytime we instantiate a cookies tree model for it.
  CannedBrowsingDataDatabaseHelper* Clone();

  // Add a database to the set of canned databases that is returned by this
  // helper.
  void AddDatabase(const GURL& origin,
                   const std::string& name,
                   const std::string& description);

  // Clear the list of canned databases.
  void Reset();

  // True if no databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored databases.
  size_t GetDatabaseCount() const;

  // Returns the current list of web databases.
  const std::set<PendingDatabaseInfo>& GetPendingDatabaseInfo();

  // BrowsingDataDatabaseHelper implementation.
  virtual void StartFetching(
      const base::Callback<void(const std::list<DatabaseInfo>&)>& callback)
          OVERRIDE;
  virtual void DeleteDatabase(const std::string& origin_identifier,
                              const std::string& name) OVERRIDE;

 private:
  virtual ~CannedBrowsingDataDatabaseHelper();

  std::set<PendingDatabaseInfo> pending_database_info_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataDatabaseHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_
