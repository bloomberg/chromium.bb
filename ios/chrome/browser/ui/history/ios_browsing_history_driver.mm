// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/ios_browsing_history_driver.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_utils.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/ui/history/history_consumer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using history::BrowsingHistoryService;

#pragma mark - IOSBrowsingHistoryDriver

IOSBrowsingHistoryDriver::IOSBrowsingHistoryDriver(
    ios::ChromeBrowserState* browser_state,
    id<HistoryConsumer> consumer)
    : browser_state_(browser_state), consumer_(consumer) {
  DCHECK(browser_state_);
}

IOSBrowsingHistoryDriver::~IOSBrowsingHistoryDriver() {
  consumer_ = nil;
}

#pragma mark - Private methods

void IOSBrowsingHistoryDriver::OnQueryComplete(
    const std::vector<BrowsingHistoryService::HistoryEntry>& results,
    const BrowsingHistoryService::QueryResultsInfo& query_results_info,
    base::OnceClosure continuation_closure) {
  [consumer_
      historyQueryWasCompletedWithResults:results
                         queryResultsInfo:query_results_info
                      continuationClosure:std::move(continuation_closure)];
}

void IOSBrowsingHistoryDriver::OnRemoveVisitsComplete() {
  // Ignored.
}

void IOSBrowsingHistoryDriver::OnRemoveVisitsFailed() {
  // Ignored.
}

void IOSBrowsingHistoryDriver::OnRemoveVisits(
    const std::vector<history::ExpireHistoryArgs>& expire_list) {
  // Ignored.
}

void IOSBrowsingHistoryDriver::HistoryDeleted() {
  [consumer_ historyWasDeleted];
}

void IOSBrowsingHistoryDriver::HasOtherFormsOfBrowsingHistory(
    bool has_other_forms,
    bool has_synced_results) {
  [consumer_ showNoticeAboutOtherFormsOfBrowsingHistory:has_other_forms];
}

bool IOSBrowsingHistoryDriver::AllowHistoryDeletions() {
  // Current reasons for suppressing history deletions are from features that
  // are not currently supported on iOS. Reasons being administrator policy and
  // supervised users.
  return true;
}

bool IOSBrowsingHistoryDriver::ShouldHideWebHistoryUrl(const GURL& url) {
  return !ios::CanAddURLToHistory(url);
}

history::WebHistoryService* IOSBrowsingHistoryDriver::GetWebHistoryService() {
  return ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_);
}

void IOSBrowsingHistoryDriver::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
    const syncer::SyncService* sync_service,
    history::WebHistoryService* history_service,
    base::Callback<void(bool)> callback) {
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service, history_service, std::move(callback));
}
