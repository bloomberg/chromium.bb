// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_SERVICE_FACADE_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_SERVICE_FACADE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/web_history_service.h"
#include "url/gurl.h"

namespace history {
struct HistoryEntry;
class HistoryService;
struct QueryOptions;
class QueryResults;
}

namespace ios {
class ChromeBrowserState;
}

@protocol HistoryServiceFacadeDelegate;

// Facade for HistoryService and WebHistoryService. Handles history querying and
// deletion actions.
class HistoryServiceFacade : public history::HistoryServiceObserver {
 public:
  // Represents the result of a query to history service.
  struct QueryResult {
    QueryResult();
    QueryResult(const QueryResult&);
    ~QueryResult();
    base::string16 query;
    base::string16 query_start_time;
    base::string16 query_end_time;
    // true if all local history from History service has been retrieved.
    bool finished;
    // true if a query to WebHistoryService has returned successfully.
    bool sync_returned;
    // true if results from WebHistoryService have been retrieved.
    bool has_synced_results;
    // true if all remote history from WebHistoryService has been retrieved.
    bool sync_finished;
    std::vector<history::HistoryEntry> entries;
  };

  // Represents a history entry removed by the client.
  struct RemovedEntry {
    RemovedEntry(const GURL& url, const base::Time& timestamp);
    RemovedEntry(const GURL& url, const std::vector<base::Time>& timestamps);
    RemovedEntry(const RemovedEntry&);
    ~RemovedEntry();
    GURL url;
    std::vector<base::Time> timestamps;
  };

  HistoryServiceFacade(ios::ChromeBrowserState* browser_state,
                       id<HistoryServiceFacadeDelegate> delegate);
  ~HistoryServiceFacade() override;

  // Performs history query with query |search_text| and |options|;
  void QueryHistory(const base::string16& search_text,
                    const history::QueryOptions& options);

  // Removes history entries in HistoryService and WebHistoryService.
  void RemoveHistoryEntries(const std::vector<RemovedEntry>& entries);

  // Queries WebHistoryService to determine whether notice about other forms
  // of browsing history should be shown. The response is returned via the
  // historyServiceFacade:shouldShowNoticeAboutOtherFormsOfBrowsingHistory:
  // delegate callback.
  void QueryOtherFormsOfBrowsingHistory();

 private:
  // The range for which to return results:
  // - ALLTIME: allows access to all the results in a paginated way.
  // - WEEK: the last 7 days.
  // - MONTH: the last calendar month.
  enum Range { ALL_TIME = 0, WEEK = 1, MONTH = 2 };

  // Callback from |web_history_timer_| when a response from web history has
  // not been received in time.
  void WebHistoryTimeout();

  // Callback from the history system when a history query has completed.
  void QueryComplete(const base::string16& search_text,
                     const history::QueryOptions& options,
                     history::QueryResults* results);

  // Callback from the WebHistoryService when a query has completed.
  void WebHistoryQueryComplete(const base::string16& search_text,
                               const history::QueryOptions& options,
                               base::TimeTicks start_time,
                               history::WebHistoryService::Request* request,
                               const base::DictionaryValue* results_value);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // Callback from history server when visits were deleted.
  void RemoveWebHistoryComplete(bool success);

  // Callback telling  whether other forms of browsing history were found
  // on the history server.
  void OtherFormsOfBrowsingHistoryQueryComplete(
      bool found_other_forms_of_browsing_history);

  // Combines the query results from the local history database and the history
  // server, and sends the combined results to the front end.
  void ReturnResultsToFrontEnd();

  // history::HistoryServiceObserver method.
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Tracker for search requests to the history service.
  base::CancelableTaskTracker query_task_tracker_;

  // The currently-executing request for synced history results.
  // Deleting the request will cancel it.
  std::unique_ptr<history::WebHistoryService::Request> web_history_request_;

  // True if there is a pending delete requests to the history service.
  bool has_pending_delete_request_;

  // Tracker for delete requests to the history service.
  base::CancelableTaskTracker delete_task_tracker_;

  // The list of URLs that are in the process of being deleted.
  std::set<GURL> urls_to_be_deleted_;

  // Information that is returned to the front end with the query results.
  QueryResult results_info_value_;

  // The list of query results received from the history service.
  std::vector<history::HistoryEntry> query_results_;

  // The list of query results received from the history server.
  std::vector<history::HistoryEntry> web_history_query_results_;

  // Timer used to implement a timeout on a Web History response.
  base::OneShotTimer web_history_timer_;

  // Observer for HistoryService.
  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  // The current browser state.
  ios::ChromeBrowserState* browser_state_;  // weak

  // Delegate for HistoryServiceFacade. Serves as client for HistoryService.
  __weak id<HistoryServiceFacadeDelegate> delegate_;

  base::WeakPtrFactory<HistoryServiceFacade> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HistoryServiceFacade);
};

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_HISTORY_SERVICE_FACADE_H_
