// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/web_history_service.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"

class BookmarkModel;
class ProfileSyncService;
class SupervisedUserService;

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler : public content::WebUIMessageHandler,
                               public content::NotificationObserver {
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

    HistoryEntry(EntryType type, const GURL& url, const base::string16& title,
                 base::Time time, const std::string& client_id,
                 bool is_search_result, const base::string16& snippet,
                 bool blocked_visit, const std::string& accept_languages);
    HistoryEntry();
    virtual ~HistoryEntry();

    // Formats this entry's URL and title and adds them to |result|.
    void SetUrlAndTitle(base::DictionaryValue* result) const;

    // Converts the entry to a DictionaryValue to be owned by the caller.
    scoped_ptr<base::DictionaryValue> ToValue(
        BookmarkModel* bookmark_model,
        SupervisedUserService* supervised_user_service,
        const ProfileSyncService* sync_service) const;

    // Comparison function for sorting HistoryEntries from newest to oldest.
    static bool SortByTimeDescending(
        const HistoryEntry& entry1, const HistoryEntry& entry2);

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
    std::set<int64> all_timestamps;

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
  virtual ~BrowsingHistoryHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Handler for the "queryHistory" message.
  void HandleQueryHistory(const base::ListValue* args);

  // Handler for the "removeVisits" message.
  void HandleRemoveVisits(const base::ListValue* args);

  // Handler for "clearBrowsingData" message.
  void HandleClearBrowsingData(const base::ListValue* args);

  // Handler for "removeBookmark" message.
  void HandleRemoveBookmark(const base::ListValue* args);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
  enum Range {
    ALL_TIME = 0,
    WEEK = 1,
    MONTH = 2
  };

  // Core implementation of history querying.
  void QueryHistory(base::string16 search_text,
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

  bool ExtractIntegerValueAtIndex(
      const base::ListValue* value, int index, int* out_int);

  // Sets the query options for a week-wide query, |offset| weeks ago.
  void SetQueryTimeInWeeks(int offset, history::QueryOptions* options);

  // Sets the query options for a monthly query, |offset| months ago.
  void SetQueryTimeInMonths(int offset, history::QueryOptions* options);

  // kAcceptLanguages pref value.
  std::string GetAcceptLanguages() const;

  content::NotificationRegistrar registrar_;

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
  base::OneShotTimer<BrowsingHistoryHandler> web_history_timer_;

  base::WeakPtrFactory<BrowsingHistoryHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

class HistoryUI : public content::WebUIController {
 public:
  explicit HistoryUI(content::WebUI* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_UI_H_
