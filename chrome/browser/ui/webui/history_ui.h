// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_

#include <string>

#include "base/string16.h"
#include "base/timer.h"
#include "base/values.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/layout.h"

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public content::WebUIMessageHandler,
                               public content::NotificationObserver {
 public:
  BrowsingHistoryHandler();
  virtual ~BrowsingHistoryHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Handler for the "queryHistory" message.
  void HandleQueryHistory(const base::ListValue* args);

  // Handler for the "removeURLsOnOneDay" message.
  void HandleRemoveURLsOnOneDay(const base::ListValue* args);

  // Handler for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // Handler for "removeBookmark" message.
  void HandleRemoveBookmark(const base::ListValue* args);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Removes duplicate visits from the given list of query results, only
  // retaining the most recent visit to a URL on a particular day. |results|
  // must already be sorted by visit time, most recent first.
  static void RemoveDuplicateResults(base::ListValue* results);

 private:
  // The range for which to return results:
  // - ALLTIME: allows access to all the results in a paginated way.
  // - WEEK: the last 7 days.
  // - MONTH: the last calendar month.
  enum Range {
    ALL_TIME = 0,
    WEEK = 1,
    MONTH = 2
  };

  // Core implementation of history querying.
  void QueryHistory(string16 search_text, const history::QueryOptions& options);

  // Creates a history query result value.
  base::DictionaryValue* CreateQueryResultValue(
      const GURL& url, const string16& title, base::Time visit_time,
      bool is_search_result, const string16& snippet);

  // Sends the accumulated results of the query to the front end, truncating
  // the number to |max_count| if necessary. If |max_count| is 0, the results
  // are not truncated.
  // If |remove_duplicates| is true, duplicate visits on the same day are
  // removed.
  void ReturnResultsToFrontEnd(bool remove_duplicates, int max_count);

  // Callback from |web_history_timer_| when a response from web history has
  // not been received in time.
  void WebHistoryTimeout();

  // Callback from the history system when a history query has completed.
  void QueryComplete(const string16& search_text,
                     const history::QueryOptions& options,
                     HistoryService::Handle request_handle,
                     history::QueryResults* results);

  // Callback from the WebHistoryService when a query has completed.
  void WebHistoryQueryComplete(const string16& search_text,
                               const history::QueryOptions& options,
                               history::WebHistoryService::Request* request,
                               const base::DictionaryValue* results_value);

  // Callback from the history system when visits were deleted.
  void RemoveComplete();

  // Callback from history server when visits were deleted.
  void RemoveWebHistoryComplete(history::WebHistoryService::Request* request,
                                bool success);

  bool ExtractIntegerValueAtIndex(
      const base::ListValue* value, int index, int* out_int);

  // Set the query options for a week-wide query, |offset| weeks ago.
  void SetQueryTimeInWeeks(int offset, history::QueryOptions* options);

  // Sets the query options for a monthly query, |offset| months ago.
  void SetQueryTimeInMonths(int offset, history::QueryOptions* options);

  content::NotificationRegistrar registrar_;

  // Consumer for search requests to the history service.
  CancelableRequestConsumerT<int, 0> history_request_consumer_;

  // The currently-executing request for synced history results.
  // Deleting the request will cancel it.
  scoped_ptr<history::WebHistoryService::Request> web_history_request_;

  // The currently-executing delete request for synced history.
  // Deleting the request will cancel it.
  scoped_ptr<history::WebHistoryService::Request> web_history_delete_request_;

  // Tracker for delete requests to the history service.
  CancelableTaskTracker delete_task_tracker_;

  // The list of URLs that are in the process of being deleted.
  std::set<GURL> urls_to_be_deleted_;

  // The info value that is returned to the front end with the query results.
  base::DictionaryValue results_info_value_;

  // The list of query results that is returned to the front end.
  base::ListValue results_value_;

  // Timer used to implement a timeout on a Web History response.
  base::OneShotTimer<BrowsingHistoryHandler> web_history_timer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

class HistoryUI : public content::WebUIController {
 public:
  explicit HistoryUI(content::WebUI* web_ui);

  // Return the URL for a given search term.
  static const GURL GetHistoryURLWithSearchText(const string16& text);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
