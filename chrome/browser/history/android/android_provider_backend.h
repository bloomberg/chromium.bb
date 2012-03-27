// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_

#include <set>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/history/android/android_cache_database.h"
#include "chrome/browser/history/android/android_history_types.h"
#include "chrome/browser/history/android/sql_handler.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "sql/statement.h"
#include "sql/transaction.h"

class BookmarkService;

namespace history {

class AndroidProviderBackend;
class AndroidURLsSQLHandler;
class HistoryDatabase;
class ThumbnailDatabase;

// This class provides the query/insert/update/remove methods to implement
// android.provider.Browser.BookmarkColumns and
// android.provider.Browser.SearchColumns API.
//
// When used it:
// a. The android_urls table is created in history database if it doesn't
//    exists.
// b. The android_cache database is created.
// c. The bookmark_cache table is created.
//
// Android_urls and android_cache database is only updated before the related
// methods are accessed. A data change will not triger the update.
//
// The android_cache database is deleted when shutdown.
class AndroidProviderBackend {
 public:
  AndroidProviderBackend(const FilePath& cache_db_name,
                         HistoryDatabase* history_db,
                         ThumbnailDatabase* thumbnail_db,
                         BookmarkService* bookmark_service,
                         HistoryBackend::Delegate* delegate);

  ~AndroidProviderBackend();

  // Bookmarks ----------------------------------------------------------------
  //
  // Runs the given query and returns the result on success, NULL on error or
  // the |projections| is empty.
  //
  // |projections| is the vector of the result columns.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| is the SQL ORDER clause.
  AndroidStatement* QueryBookmarks(
      const std::vector<BookmarkRow::BookmarkColumnID>& projections,
      const std::string& selection,
      const std::vector<string16>& selection_args,
      const std::string& sort_order);

  // Runs the given update and returns the number of the updated rows in
  // |update_count| and return true on success, false on error.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  bool UpdateBookmarks(const BookmarkRow& row,
                       const std::string& selection,
                       const std::vector<string16>& selection_args,
                       int* update_count);

  // Inserts the given values and returns the URLID of the inserted row.
  AndroidURLID InsertBookmark(const BookmarkRow& values);

  // Deletes the specified rows and returns the number of the deleted rows in
  // |deleted_count|.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // if |selection| is empty all history and bookmarks are deleted.
  bool DeleteBookmarks(const std::string& selection,
                       const std::vector<string16>& selection_args,
                       int* deleted_count);

 private:
  friend class AndroidProviderBackendTest;

  FRIEND_TEST_ALL_PREFIXES(AndroidProviderBackendTest, UpdateTables);
  FRIEND_TEST_ALL_PREFIXES(AndroidProviderBackendTest, UpdateBookmarks);

  struct HistoryNotification {
    HistoryNotification(int type, HistoryDetails* detail);
    ~HistoryNotification();

    int type;
    // The ownership of the HistoryDetails pointer is transfered to |detail|.
    HistoryDetails* detail;
  };
  typedef std::vector<HistoryNotification> HistoryNotifications;

  // The scoped transaction for AndroidProviderBackend.
  //
  // The new transactions are started automatically in both history and
  // thumbnail database and could be a nesting transaction, if so, rolling back
  // of this transaction will cause the exsting and subsequent nesting
  // transactions failed.
  //
  // Commit() is used to commit the transaction, otherwise the transaction will
  // be rolled back when the object is out of scope. This transaction could
  // failed even the commit() is called if it is in a transaction that has been
  // rolled back or the subsequent transaction in the same outermost
  // transaction would be rolled back latter.
  //
  class ScopedTransaction {
   public:
    ScopedTransaction(HistoryDatabase* history_db,
                      ThumbnailDatabase* thumbnail_db);
    ~ScopedTransaction();

    // Commit the transaction.
    void Commit();

   private:
    HistoryDatabase* history_db_;
    ThumbnailDatabase* thumbnail_db_;
    // Whether the transaction was committed.
    bool committed_;

    DISALLOW_COPY_AND_ASSIGN(ScopedTransaction);
  };

  // Runs the given update and returns the number of updated rows in
  // |update_count| and return true on success, false on error.
  //
  // The notifications of change is returned in |notifications|.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  bool UpdateBookmarks(const BookmarkRow& row,
                       const std::string& selection,
                       const std::vector<string16>& selection_args,
                       int* update_count,
                       HistoryNotifications* notifications);

  // Inserts the given values and returns the URLID of the inserted row.
  // The notifications of change is returned in |notifications|.
  AndroidURLID InsertBookmark(const BookmarkRow& values,
                              HistoryNotifications* notifications);

  // Deletes the specified rows and returns the number of the deleted rows in
  // |deleted_count|.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // The notifications of change is returned in |notifications|.
  //
  // if |selection| is empty all history and bookmarks are deleted.
  bool DeleteBookmarks(const std::string& selection,
                       const std::vector<string16>& selection_args,
                       int* deleted_count,
                       HistoryNotifications* notifications);

  // Initializes and updates tables if necessary.
  bool EnsureInitializedAndUpdated();

  // Initializes AndroidProviderBackend.
  bool Init();

  // Update android_urls and bookmark_cache table if it is necessary.
  bool UpdateTables();

  // Update the android_urls and bookmark_cache for visited urls.
  bool UpdateVisitedURLs();

  // Update the android_urls for removed urls.
  bool UpdateRemovedURLs();

  // Update the bookmark_cache table with bookmarks.
  bool UpdateBookmarks();

  // Update the bookmark_cache table for favicon.
  bool UpdateFavicon();

  // Append the specified result columns in |projections| to the given
  // |result_column|.
  // To support the lazy binding, the index of favicon column will be
  // returned if it exists, otherwise returns -1.
  int AppendBookmarkResultColumn(
      const std::vector<BookmarkRow::BookmarkColumnID>& projections,
      std::string* result_column);

  // Runs the given query on |virtual_table| and returns true if succeeds, the
  // selected URLID and url are returned in |rows|.
  bool GetSelectedURLs(const std::string& selection,
                       const std::vector<string16>& selection_args,
                       const char* virtual_table,
                       TableIDRows* rows);

  // Simulates update url by deleting the previous URL and creating a new one.
  // Return true on success.
  bool SimulateUpdateURL(const BookmarkRow& row,
                         const TableIDRows& ids,
                         HistoryNotifications* notifications);

  // Query bookmark without sync the tables. It should be used after syncing
  // tables.
  AndroidStatement* QueryBookmarksInternal(
      const std::vector<BookmarkRow::BookmarkColumnID>& projections,
      const std::string& selection,
      const std::vector<string16>& selection_args,
      const std::string& sort_order);

  void BroadcastNotifications(const HistoryNotifications& notifications);

  // SQLHandlers for different tables.
  scoped_ptr<SQLHandler> urls_handler_;
  scoped_ptr<SQLHandler> visit_handler_;
  scoped_ptr<SQLHandler> android_urls_handler_;
  scoped_ptr<SQLHandler> favicon_handler_;
  scoped_ptr<SQLHandler> bookmark_model_handler_;

  // The vector of all handlers
  std::vector<SQLHandler*> sql_handlers_;

  // Android cache database filename.
  const FilePath android_cache_db_filename_;

  // The history db's connection.
  sql::Connection* db_;

  HistoryDatabase* history_db_;

  ThumbnailDatabase* thumbnail_db_;

  BookmarkService* bookmark_service_;

  // Whether AndroidProviderBackend has been initialized.
  bool initialized_;

  HistoryBackend::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(AndroidProviderBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_PROVIDER_BACKEND_H_
