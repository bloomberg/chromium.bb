// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_CACHE_DATABASE_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_CACHE_DATABASE_H_
#pragma once

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/history/android/android_history_types.h"
#include "sql/connection.h"
#include "sql/init_status.h"

namespace history {

// This database is used to support Android ContentProvider APIs.
// It will be created only when it used, and deleted by HistoryBackend when
// history system shutdown.
class AndroidCacheDatabase {
 public:
  AndroidCacheDatabase();
  virtual ~AndroidCacheDatabase();

  // Creates the database, deletes existing one if any; Also attach it to the
  // database returned by GetDB(). Returns sql::INIT_OK on success, otherwise
  // sql::INIT_FAILURE returned.
  sql::InitStatus InitAndroidCacheDatabase(const FilePath& db_name);

  // Adds a row to the bookmark_cache table. Returns true on success.
  bool AddBookmarkCacheRow(const base::Time& created_time,
                           const base::Time& last_visit_time,
                           URLID url_id);

  // Clears all rows in the bookmark_cache table; Returns true on success.
  bool ClearAllBookmarkCache();

  // Marks the given |url_ids| as bookmarked; Returns true on success.
  bool MarkURLsAsBookmarked(const std::vector<URLID>& url_id);

  // Set the given |url_id|'s favicon column to |favicon_id|. Returns true on
  // success.
  bool SetFaviconID(URLID url_id, FaviconID favicon_id);

 protected:
  // Returns the database for the functions in this interface. The decendent of
  // this class implements these functions to return its objects.
  virtual sql::Connection& GetDB() = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(AndroidCacheDatabaseTest,
                           CreateDatabase);

  // Creates the database and make it ready for attaching; Returns true on
  // success.
  bool CreateDatabase(const FilePath& db_name);

  // Creates the bookmark_cache table in attached DB; Returns true on success.
  // The created_time, last_visit_time, favicon_id and bookmark are stored.
  //
  // The created_time and last_visit_time are cached because Android use the
  // millisecond for the time unit, and we don't want to convert it in the
  // runtime for it requires to parsing the SQL.
  //
  // The favicon_id is also cached because it is in thumbnail database. Its
  // default value is set to null as the type of favicon column in Android APIs
  // is blob. To use default value null, we can support client query by
  // 'WHERE favicon IS NULL'.
  //
  // Bookmark column is used to indicate whether the url is bookmarked.
  bool CreateBookmarkCacheTable();

  // Attachs to history database; Returns true on success.
  bool Attach();

  // Does the real attach. Returns true on success.
  bool DoAttach();

  FilePath db_name_;

  DISALLOW_COPY_AND_ASSIGN(AndroidCacheDatabase);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_CACHE_DATABASE_H_
