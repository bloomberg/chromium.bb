// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_
#define CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/android/android_history_types.h"
#include "sql/statement.h"

class Profile;

// This class provides the methods to communicate with history backend service
// for the Android content provider.
// The methods of this class must run on the UI thread to cooperate with the
// BookmarkModel task posted in the DB thread.
class AndroidHistoryProviderService : public CancelableRequestProvider {
 public:
  explicit AndroidHistoryProviderService(Profile* profile);
  virtual ~AndroidHistoryProviderService();

  // The callback definitions ------------------------------------------------
  typedef base::Callback<void(
                    Handle,                // handle
                    bool,                  // true if the query succeeded.
                    history::AndroidStatement*)>  // the result of query
                    QueryCallback;
  typedef CancelableRequest<QueryCallback> QueryRequest;

  typedef base::Callback<void(
                    Handle,                // handle
                    bool,                  // true if the update succeeded.
                    int)>                  // the number of row updated.
                    UpdateCallback;
  typedef CancelableRequest<UpdateCallback> UpdateRequest;

  typedef base::Callback<void(
                    Handle,                // handle
                    bool,                  // true if the insert succeeded.
                    int64)>                // the id of inserted row.
                    InsertCallback;
  typedef CancelableRequest<InsertCallback> InsertRequest;

  typedef base::Callback<void(
                    Handle,                // handle
                    bool,                  // true if the deletion succeeded.
                    int)>                  // the number of row deleted.
                    DeleteCallback;
  typedef CancelableRequest<DeleteCallback> DeleteRequest;

  typedef base::Callback<void(
                    Handle,                // handle
                    int)>                  // the new position.
                    MoveStatementCallback;
  typedef CancelableRequest<MoveStatementCallback> MoveStatementRequest;

  // History and Bookmarks ----------------------------------------------------
  //
  // Runs the given query on history backend, and invokes the |callback| to
  // return the result.
  //
  // |projections| is the vector of the result columns.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| is the SQL ORDER clause.
  Handle QueryHistoryAndBookmarks(
      const std::vector<history::HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<string16>& selection_args,
      const std::string& sort_order,
      CancelableRequestConsumerBase* consumer,
      const QueryCallback& callback);

  // Runs the given update and the number of the row updated is returned to the
  // |callback| on success.
  //
  // |row| is the value to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  Handle UpdateHistoryAndBookmarks(const history::HistoryAndBookmarkRow& row,
                                   const std::string& selection,
                                   const std::vector<string16>& selection_args,
                                   CancelableRequestConsumerBase* consumer,
                                   const UpdateCallback& callback);

  // Deletes the specified rows and invokes the |callback| to return the number
  // of row deleted on success.
  //
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for the WHERE clause.
  //
  // if |selection| is empty all history and bookmarks are deleted.
  Handle DeleteHistoryAndBookmarks(const std::string& selection,
                                   const std::vector<string16>& selection_args,
                                   CancelableRequestConsumerBase* consumer,
                                   const DeleteCallback& callback);

  // Inserts the given values into history backend, and invokes the |callback|
  // to return the result.
  Handle InsertHistoryAndBookmark(const history::HistoryAndBookmarkRow& values,
                                  CancelableRequestConsumerBase* consumer,
                                  const InsertCallback& callback);

  // Deletes the matched history and invokes |callback| to return the number of
  // the row deleted from the |callback|.
  Handle DeleteHistory(const std::string& selection,
                       const std::vector<string16>& selection_args,
                       CancelableRequestConsumerBase* consumer,
                       const DeleteCallback& callback);

  // Statement ----------------------------------------------------------------
  // Moves the statement's current row from |current_pos| to |destination| in DB
  // thread. The new position is returned to the callback. The result supplied
  // the callback is constrained by the number of rows might.
  Handle MoveStatement(history::AndroidStatement* statement,
                       int current_pos,
                       int destination,
                       CancelableRequestConsumerBase* consumer,
                       const MoveStatementCallback& callback);

  // Closes the statement in db thread. The AndroidHistoryProviderService takes
  // the ownership of |statement|.
  void CloseStatement(history::AndroidStatement* statement);

  // Search term --------------------------------------------------------------
  // Inserts the given values and returns the SearchTermID of the inserted row
  // from the |callback| on success.
  Handle InsertSearchTerm(const history::SearchRow& row,
                          CancelableRequestConsumerBase* consumer,
                          const InsertCallback& callback);

  // Runs the given update and returns the number of the update rows from the
  // |callback| on success.
  //
  // |row| is the value need to update.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  Handle UpdateSearchTerms(const history::SearchRow& row,
                           const std::string& selection,
                           const std::vector<string16>& selection_args,
                           CancelableRequestConsumerBase* consumer,
                           const UpdateCallback& callback);

  // Deletes the matched rows and the number of deleted rows is returned from
  // the |callback| on success.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  //
  // if |selection| is empty all search be deleted.
  Handle DeleteSearchTerms(const std::string& selection,
                           const std::vector<string16>& selection_args,
                           CancelableRequestConsumerBase* consumer,
                           const DeleteCallback& callback);

  // Returns the result of the given query from the |callback|.
  // |projections| specifies the result columns, can not be empty, otherwise
  // NULL is returned.
  // |selection| is the SQL WHERE clause without 'WHERE'.
  // |selection_args| is the arguments for WHERE clause.
  // |sort_order| the SQL ORDER clause.
  Handle QuerySearchTerms(
      const std::vector<history::SearchRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<string16>& selection_args,
      const std::string& sort_order,
      CancelableRequestConsumerBase* consumer,
      const QueryCallback& callback);

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(AndroidHistoryProviderService);
};

#endif  // CHROME_BROWSER_HISTORY_ANDROID_ANDROID_HISTORY_PROVIDER_SERVICE_H_
