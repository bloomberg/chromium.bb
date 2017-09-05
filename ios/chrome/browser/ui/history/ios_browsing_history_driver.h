// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_
#define IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/browsing_history_driver.h"
#include "components/history/core/browser/browsing_history_service.h"
#include "url/gurl.h"

namespace history {
class HistoryService;
}

namespace ios {
class ChromeBrowserState;
}

// Defines methods to manage history query results and deletion actions.
@protocol BrowsingHistoryDriverDelegate<NSObject>

// Notifies the delegate that the result of a history query has been retrieved.
// Entries in |result| are already sorted.
- (void)onQueryCompleteWithResults:
            (const std::vector<history::BrowsingHistoryService::HistoryEntry>&)
                results
                  queryResultsInfo:
                      (const history::BrowsingHistoryService::QueryResultsInfo&)
                          queryResultsInfo
               continuationClosure:(base::OnceClosure)continuationClosure;

// Notifies the delegate that history entries have been deleted by a different
// client and that the UI should be updated.
- (void)didObserverHistoryDeletion;

// Indicates to the delegate whether to show notice about other forms of
// browsing history.
- (void)shouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)shouldShowNotice;

@end

// A simple implementation of BrowsingHistoryServiceHandler that delegates to
// objective-c object BrowsingHistoryDriverDelegate for most actions.
class IOSBrowsingHistoryDriver : public history::BrowsingHistoryDriver {
 public:
  IOSBrowsingHistoryDriver(ios::ChromeBrowserState* browser_state,
                           id<BrowsingHistoryDriverDelegate> delegate);
  ~IOSBrowsingHistoryDriver() override;

 private:
  // history::BrowsingHistoryDriver implementation.
  void OnQueryComplete(
      const std::vector<history::BrowsingHistoryService::HistoryEntry>& results,
      const history::BrowsingHistoryService::QueryResultsInfo&
          query_results_info,
      base::OnceClosure continuation_closure) override;
  void OnRemoveVisitsComplete() override;
  void OnRemoveVisitsFailed() override;
  void OnRemoveVisits(
      const std::vector<history::ExpireHistoryArgs>& expire_list) override;
  void HistoryDeleted() override;
  void HasOtherFormsOfBrowsingHistory(bool has_other_forms,
                                      bool has_synced_results) override;
  bool AllowHistoryDeletions() override;
  bool ShouldHideWebHistoryUrl(const GURL& url) override;
  history::WebHistoryService* GetWebHistoryService() override;
  void ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      const syncer::SyncService* sync_service,
      history::WebHistoryService* history_service,
      base::Callback<void(bool)> callback) override;

  // The current browser state.
  ios::ChromeBrowserState* browser_state_;  // weak

  // Delegate for IOSBrowsingHistoryDriver. Serves as client for HistoryService.
  __weak id<BrowsingHistoryDriverDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(IOSBrowsingHistoryDriver);
};

#endif  // IOS_CHROME_BROWSER_UI_HISTORY_IOS_BROWSING_HISTORY_DRIVER_H_
