// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_DATABASE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_DATABASE_HELPER_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "webkit/database/database_tracker.h"

class Profile;

// This class fetches database information in the WEBKIT thread, and notifies
// the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
// The client must call CancelNotification() if it's destroyed before the
// callback is notified.
class BrowsingDataDatabaseHelper
    : public base::RefCountedThreadSafe<BrowsingDataDatabaseHelper> {
 public:
  // Contains detailed information about a web database.
  struct DatabaseInfo {
    DatabaseInfo();
    DatabaseInfo(const std::string& host,
                 const std::string& database_name,
                 const std::string& origin_identifier,
                 const std::string& description,
                 const std::string& origin,
                 int64 size,
                 base::Time last_modified);
    ~DatabaseInfo();

    bool IsFileSchemeData();

    std::string host;
    std::string database_name;
    std::string origin_identifier;
    std::string description;
    std::string origin;
    int64 size;
    base::Time last_modified;
  };

  explicit BrowsingDataDatabaseHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(
      Callback1<const std::vector<DatabaseInfo>& >::Type* callback);

  // Cancels the notification callback (i.e., the window that created it no
  // longer exists).
  // This must be called only in the UI thread.
  virtual void CancelNotification();

  // Requests a single database to be deleted in the WEBKIT thread. This must be
  // called in the UI thread.
  virtual void DeleteDatabase(const std::string& origin,
                              const std::string& name);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataDatabaseHelper>;
  virtual ~BrowsingDataDatabaseHelper();

  // Notifies the completion callback. This must be called in the UI thread.
  void NotifyInUIThread();

  // This only mutates in the WEBKIT thread.
  std::vector<DatabaseInfo> database_info_;

  // This only mutates on the UI thread.
  scoped_ptr<Callback1<const std::vector<DatabaseInfo>& >::Type >
      completion_callback_;

  // Indicates whether or not we're currently fetching information:
  // it's true when StartFetching() is called in the UI thread, and it's reset
  // after we notify the callback in the UI thread.
  // This only mutates on the UI thread.
  bool is_fetching_;

 private:
  // Enumerates all databases. This must be called in the WEBKIT thread.
  void FetchDatabaseInfoInWebKitThread();

  // Delete a single database file. This must be called in the WEBKIT thread.
  void DeleteDatabaseInWebKitThread(const std::string& origin,
                                  const std::string& name);

  scoped_refptr<webkit_database::DatabaseTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataDatabaseHelper);
};

// This class is a thin wrapper around BrowsingDataDatabaseHelper that does not
// fetch its information from the database tracker, but gets them passed as
// a parameter during construction.
class CannedBrowsingDataDatabaseHelper : public BrowsingDataDatabaseHelper {
 public:
  explicit CannedBrowsingDataDatabaseHelper(Profile* profile);

  // Add a database to the set of canned databases that is returned by this
  // helper.
  void AddDatabase(const GURL& origin,
                   const std::string& name,
                   const std::string& description);

  // Clear the list of canned databases.
  void Reset();

  // True if no databases are currently stored.
  bool empty() const;

  // BrowsingDataDatabaseHelper methods.
  virtual void StartFetching(
      Callback1<const std::vector<DatabaseInfo>& >::Type* callback);
  virtual void CancelNotification() {}

 private:
  struct PendingDatabaseInfo {
    PendingDatabaseInfo();
    PendingDatabaseInfo(const GURL& origin,
                        const std::string& name,
                        const std::string& description);
    ~PendingDatabaseInfo();

    GURL origin;
    std::string name;
    std::string description;
  };

  virtual ~CannedBrowsingDataDatabaseHelper() {}

  // Converts the pending database info structs to database info structs.
  void ConvertInfoInWebKitThread();

  // Used to protect access to pending_database_info_.
  mutable base::Lock lock_;

  // This may mutate on WEBKIT and UI threads.
  std::vector<PendingDatabaseInfo> pending_database_info_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataDatabaseHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_DATABASE_HELPER_H_
