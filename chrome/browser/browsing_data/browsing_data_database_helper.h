// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/common/database/database_identifier.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
struct StorageUsageInfo;
}

class Profile;

// This class fetches database information in the FILE thread, and notifies
// the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class BrowsingDataDatabaseHelper
    : public base::RefCountedThreadSafe<BrowsingDataDatabaseHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit BrowsingDataDatabaseHelper(Profile* profile);

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Deletes all databases associated with an origin. This must be called in the
  // UI thread.
  virtual void DeleteDatabase(const url::Origin& origin);

 protected:
  friend class base::RefCountedThreadSafe<BrowsingDataDatabaseHelper>;
  virtual ~BrowsingDataDatabaseHelper();

 private:
  scoped_refptr<storage::DatabaseTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataDatabaseHelper);
};

// This class is a thin wrapper around BrowsingDataDatabaseHelper that does not
// fetch its information from the database tracker, but gets them passed by
// a call when accessed.
class CannedBrowsingDataDatabaseHelper : public BrowsingDataDatabaseHelper {
 public:
  explicit CannedBrowsingDataDatabaseHelper(Profile* profile);

  // Add a database to the set of canned databases that is returned by this
  // helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned databases.
  void Reset();

  // True if no databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored databases.
  size_t GetCount() const;

  // Returns the current list of web databases.
  const std::set<url::Origin>& GetOrigins();

  // BrowsingDataDatabaseHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteDatabase(const url::Origin& origin) override;

 private:
  ~CannedBrowsingDataDatabaseHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedBrowsingDataDatabaseHelper);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_DATABASE_HELPER_H_
