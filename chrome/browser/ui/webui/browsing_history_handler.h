// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BROWSING_HISTORY_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_BROWSING_HISTORY_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chrome/browser/history/browsing_history_service_handler.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace history {
struct QueryOptions;
}  // namespace history

// The handler for Javascript messages related to the "history" view.
class BrowsingHistoryHandler :
    public content::WebUIMessageHandler,
    public BrowsingHistoryServiceHandler {
 public:
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

  // BrowsingHistoryServiceHandler implementation.
  void OnQueryComplete(
      std::vector<BrowsingHistoryService::HistoryEntry>* results,
      BrowsingHistoryService::QueryResultsInfo* query_results_info) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(
      bool has_other_forms, bool has_synced_results) override;

  // For tests.
  void set_clock(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest,
                           ObservingWebHistoryDeletions);
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest, SetQueryTimeInWeeks);
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest, SetQueryTimeInMonths);
  FRIEND_TEST_ALL_PREFIXES(BrowsingHistoryHandlerTest, MdTruncatesTitles);

  // The range for which to return results:
  // - ALLTIME: allows access to all the results in a paginated way.
  // - WEEK: the last 7 days.
  // - MONTH: the last calendar month.
  enum Range {
    ALL_TIME = 0,
    WEEK = 1,
    MONTH = 2
  };

  bool ExtractIntegerValueAtIndex(
      const base::ListValue* value, int index, int* out_int);

  // Sets the query options for a week-wide query, |offset| weeks ago.
  void SetQueryTimeInWeeks(int offset, history::QueryOptions* options);

  // Sets the query options for a monthly query, |offset| months ago.
  void SetQueryTimeInMonths(int offset, history::QueryOptions* options);

  // The clock used to vend times.
  std::unique_ptr<base::Clock> clock_;

  std::unique_ptr<BrowsingHistoryService> browsing_history_service_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BROWSING_HISTORY_HANDLER_H_
