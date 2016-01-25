// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/web_history_service.h"
#include "ios/public/provider/web/web_ui_ios_message_handler.h"
#include "url/gurl.h"

class ProfileSyncService;
class SupervisedUserService;

namespace bookmarks {
class BookmarkModel;
}

namespace history {
class HistoryService;
struct QueryOptions;
class QueryResults;
}

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public web::WebUIIOSMessageHandler,
                               public history::HistoryServiceObserver {
 public:
  // Represents a history entry to be shown to the user, representing either
  // a local or remote visit. A single entry can represent multiple visits,
  // since only the most recent visit on a particular day is shown.
  struct HistoryEntry {
    // Values indicating whether an entry represents only local visits, only
    // remote visits, or a mixture of both.
    enum EntryType {
      EMPTY_ENTRY = 0,
      LOCAL_ENTRY,
      REMOTE_ENTRY,
      COMBINED_ENTRY
    };

    HistoryEntry(EntryType type,
                 const GURL& url,
                 const base::string16& title,
                 base::Time time,
                 const std::string& client_id,
                 bool is_search_result,
                 const base::string16& snippet,
                 bool blocked_visit,
                 const std::string& accept_languages);
    HistoryEntry();
    virtual ~HistoryEntry();

    // Formats this entry's URL and title and adds them to |result|.
    void SetUrlAndTitle(base::DictionaryValue* result) const;

    // Converts the entry to a DictionaryValue to be owned by the caller.
    scoped_ptr<base::DictionaryValue> ToValue(
        bookmarks::BookmarkModel* bookmark_model,
        SupervisedUserService* supervised_user_service,
        const ProfileSyncService* sync_service) const;

    // Comparison function for sorting HistoryEntries from newest to oldest.
    static bool SortByTimeDescending(const HistoryEntry& entry1,
                                     const HistoryEntry& entry2);

    // The type of visits this entry represents: local, remote, or both.
    EntryType entry_type;

    GURL url;
    base::string16 title;  // Title of the entry. May be empty.

    // The time of the entry. Usually this will be the time of the most recent
    // visit to |url| on a particular day as defined in the local timezone.
    base::Time time;

    // The sync ID of the client on which the most recent visit occurred.
    std::string client_id;

    // Timestamps of all local or remote visits the same URL on the same day.
    std::set<int64_t> all_timestamps;

    // If true, this entry is a search result.
    bool is_search_result;

    // The entry's search snippet, if this entry is a search result.
    base::string16 snippet;

    // Whether this entry was blocked when it was attempted.
    bool blocked_visit;

    // kAcceptLanguages pref value.
    std::string accept_languages;
  };

  BrowsingHistoryHandler();
  ~BrowsingHistoryHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Handler for the "queryHistory" message.
  void HandleQueryHistory(const base::ListValue* args);

  // Handler for the "removeVisits" message.
  void HandleRemoveVisits(const base::ListValue* args);

  // Handler for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // Handler for "removeBookmark" message.
  void HandleRemoveBookmark(const base::ListValue* args);

  // Merges duplicate entries from the query results, only retaining the most
  // recent visit to a URL on a particular day. That visit contains the
  // timestamps of the other visits.
  static void MergeDuplicateResults(
      std::vector<BrowsingHistoryHandler::HistoryEntry>* results);

 private:
  // The range for which to return results:
  // - ALLTIME: allows access to all the results in a paginated way.
  // - WEEK: the last 7 days.
  // - MONTH: the last calendar month.
  enum Range { ALL_TIME = 0, WEEK = 1, MONTH = 2 };

  // Core implementation of history querying.
  void QueryHistory(const base::string16& search_text,
                    const history::QueryOptions& options);

  // Combines the query results from the local history database and the history
  // server, and sends the combined results to the front end.
  void ReturnResultsToFrontEnd();

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

  bool ExtractIntegerValueAtIndex(const base::ListValue* value,
                                  int index,
                                  int* out_int);

  // Sets the query options for a week-wide query, |offset| weeks ago.
  void SetQueryTimeInWeeks(int offset, history::QueryOptions* options);

  // Sets the query options for a monthly query, |offset| months ago.
  void SetQueryTimeInMonths(int offset, history::QueryOptions* options);

  // kAcceptLanguages pref value.
  std::string GetAcceptLanguages() const;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     bool all_history,
                     bool expired,
                     const history::URLRows& deleted_rows,
                     const std::set<GURL>& favicon_urls) override;

  // Tracker for search requests to the history service.
  base::CancelableTaskTracker query_task_tracker_;

  // The currently-executing request for synced history results.
  // Deleting the request will cancel it.
  scoped_ptr<history::WebHistoryService::Request> web_history_request_;

  // True if there is a pending delete requests to the history service.
  bool has_pending_delete_request_;

  // Tracker for delete requests to the history service.
  base::CancelableTaskTracker delete_task_tracker_;

  // The list of URLs that are in the process of being deleted.
  std::set<GURL> urls_to_be_deleted_;

  // The info value that is returned to the front end with the query results.
  base::DictionaryValue results_info_value_;

  // The list of query results received from the history service.
  std::vector<HistoryEntry> query_results_;

  // The list of query results received from the history server.
  std::vector<HistoryEntry> web_history_query_results_;

  // Timer used to implement a timeout on a Web History response.
  base::OneShotTimer web_history_timer_;

  ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
      history_service_observer_;

  base::WeakPtrFactory<BrowsingHistoryHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

#endif  // IOS_CHROME_BROWSER_UI_WEBUI_HISTORY_BROWSING_HISTORY_HANDLER_H_
