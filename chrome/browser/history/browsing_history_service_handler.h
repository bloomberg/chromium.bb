// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_BROWSING_HISTORY_SERVICE_HANDLER_H_
#define CHROME_BROWSER_HISTORY_BROWSING_HISTORY_SERVICE_HANDLER_H_

#include "chrome/browser/history/browsing_history_service.h"

// Interface for handling calls from the BrowsingHistoryService.
class BrowsingHistoryServiceHandler {
 public:
  // Callback for QueryHistory().
  virtual void OnQueryComplete(
      std::vector<BrowsingHistoryService::HistoryEntry>* results,
      BrowsingHistoryService::QueryResultsInfo* query_results_info) = 0;

  // Callback for RemoveVisits().
  virtual void OnRemoveVisitsComplete() = 0;

  // Callback for RemoveVisits() that fails.
  virtual void OnRemoveVisitsFailed() = 0;

  // Called when HistoryService or WebHistoryService deletes one or more
  // items.
  virtual void HistoryDeleted() = 0;

  // Whether other forms of browsing history were found on the history
  // service.
  virtual void HasOtherFormsOfBrowsingHistory(
      bool has_other_forms, bool has_synced_results) = 0;

 protected:
  BrowsingHistoryServiceHandler() {}
  virtual ~BrowsingHistoryServiceHandler() {}

  DISALLOW_COPY_AND_ASSIGN(BrowsingHistoryServiceHandler);
};

#endif  // CHROME_BROWSER_HISTORY_BROWSING_HISTORY_SERVICE_HANDLER_H_
