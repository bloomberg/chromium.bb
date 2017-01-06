// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"

#import <Foundation/Foundation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state_user_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - ReadingListWebStateObserverUserDataWrapper

namespace {
// The key under which ReadingListWebStateObserverUserDataWrapper are stored in
// a WebState's user data.
const void* const kObserverKey = &kObserverKey;
}  // namespace

// Wrapper class used to associated ReadingListWebStateObserver with their
// WebStates.
class ReadingListWebStateObserverUserDataWrapper
    : public base::SupportsUserData::Data {
 public:
  static ReadingListWebStateObserverUserDataWrapper* FromWebState(
      web::WebState* web_state,
      ReadingListModel* reading_list_model) {
    DCHECK(web_state);
    ReadingListWebStateObserverUserDataWrapper* wrapper =
        static_cast<ReadingListWebStateObserverUserDataWrapper*>(
            web_state->GetUserData(kObserverKey));
    if (!wrapper) {
      auto newDataWrapper =
          base::MakeUnique<ReadingListWebStateObserverUserDataWrapper>(
              web_state, reading_list_model);
      wrapper = newDataWrapper.get();
      web_state->SetUserData(kObserverKey, newDataWrapper.release());
    }
    return wrapper;
  }

  ReadingListWebStateObserverUserDataWrapper(
      web::WebState* web_state,
      ReadingListModel* reading_list_model)
      : observer_(web_state, reading_list_model) {
    DCHECK(web_state);
  }

  ReadingListWebStateObserver* observer() { return &observer_; }

 private:
  ReadingListWebStateObserver observer_;
};

#pragma mark - ReadingListWebStateObserver

ReadingListWebStateObserver* ReadingListWebStateObserver::FromWebState(
    web::WebState* web_state,
    ReadingListModel* reading_list_model) {
  return ReadingListWebStateObserverUserDataWrapper::FromWebState(
             web_state, reading_list_model)
      ->observer();
}

ReadingListWebStateObserver::~ReadingListWebStateObserver() {
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
  }
}

ReadingListWebStateObserver::ReadingListWebStateObserver(
    web::WebState* web_state,
    ReadingListModel* reading_list_model)
    : web::WebStateObserver(web_state),
      reading_list_model_(reading_list_model),
      last_load_result_(web::PageLoadCompletionStatus::SUCCESS) {
  reading_list_model_->AddObserver(this);
  DCHECK(web_state);
  DCHECK(reading_list_model_);
}

bool ReadingListWebStateObserver::ShouldObserveItem(
    web::NavigationItem* item) const {
  if (!item) {
    return false;
  }

  return !reading_list::IsOfflineURL(item->GetURL());
}

bool ReadingListWebStateObserver::IsUrlAvailableOffline(const GURL& url) const {
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  return entry && entry->DistilledState() == ReadingListEntry::PROCESSED;
}

void ReadingListWebStateObserver::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_state()->IsLoading()) {
    DidStartLoading();
    return;
  }
  if (last_load_result_ == web::PageLoadCompletionStatus::SUCCESS ||
      web_state()->IsShowingWebInterstitial()) {
    return;
  }
  // An error page is being displayed.
  web::NavigationManager* manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = manager->GetLastCommittedItem();
  if (!ShouldObserveItem(item)) {
    return;
  }
  const GURL& currentURL = item->GetVirtualURL();
  if (IsUrlAvailableOffline(currentURL)) {
    pending_url_ = currentURL;
    LoadOfflineReadingListEntry();
    StopCheckingProgress();
  }
}

void ReadingListWebStateObserver::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  StopCheckingProgress();
  reading_list_model_->RemoveObserver(this);
  reading_list_model_ = nullptr;
  web::WebState* local_web_state = web_state();
  Observe(nullptr);
  local_web_state->RemoveUserData(kObserverKey);
}

void ReadingListWebStateObserver::DidStartLoading() {
  StartCheckingLoading();
}

void ReadingListWebStateObserver::StartCheckingLoading() {
  DCHECK(reading_list_model_);
  DCHECK(web_state());
  if (!reading_list_model_->loaded() ||
      web_state()->IsShowingWebInterstitial()) {
    StopCheckingProgress();
    return;
  }

  web::NavigationManager* manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();

  // Manager->GetPendingItem() returns null on reload.
  // TODO(crbug.com/676129): Remove this workaround once GetPendingItem()
  // returns the correct value on reload.
  if (!item) {
    item = manager->GetLastCommittedItem();
  }

  if (!ShouldObserveItem(item)) {
    StopCheckingProgress();
    return;
  }

  pending_url_ = item->GetVirtualURL();
  if (!IsUrlAvailableOffline(pending_url_)) {
    // No need to launch the timer as there is no offline version to show.
    // Track |pending_url_| to mark the entry as read in case of a successful
    // load.
    return;
  }
  try_number_ = 0;
  timer_.reset(new base::Timer(false, true));
  const base::TimeDelta kDelayUntilLoadingProgressIsChecked =
      base::TimeDelta::FromSeconds(1);
  timer_->Start(
      FROM_HERE, kDelayUntilLoadingProgressIsChecked,
      base::Bind(
          &ReadingListWebStateObserver::VerifyIfReadingListEntryStartedLoading,
          base::Unretained(this)));
}

void ReadingListWebStateObserver::PageLoaded(
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK(reading_list_model_);
  DCHECK(web_state());
  last_load_result_ = load_completion_status;
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  if (!item || !pending_url_.is_valid()) {
    StopCheckingProgress();
    return;
  }

  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    reading_list_model_->SetReadStatus(pending_url_, true);
    UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", false);
  } else {
    LoadOfflineReadingListEntry();
  }
  StopCheckingProgress();
}

void ReadingListWebStateObserver::WebStateDestroyed() {
  StopCheckingProgress();
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  web_state()->RemoveUserData(kObserverKey);
}

void ReadingListWebStateObserver::StopCheckingProgress() {
  pending_url_ = GURL::EmptyGURL();
  timer_.reset();
}

void ReadingListWebStateObserver::VerifyIfReadingListEntryStartedLoading() {
  if (!pending_url_.is_valid()) {
    StopCheckingProgress();
    return;
  }
  web::NavigationManager* manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();

  // Manager->GetPendingItem() returns null on reload.
  // TODO(crbug.com/676129): Remove this workaround once GetPendingItem()
  // returns the correct value on reload.
  if (!item) {
    item = manager->GetLastCommittedItem();
  }
  if (!item || !pending_url_.is_valid() ||
      !IsUrlAvailableOffline(pending_url_)) {
    StopCheckingProgress();
    return;
  }
  try_number_++;
  double progress = web_state()->GetLoadingProgress();
  const double kMinimumExpectedProgressPerStep = 0.25;
  if (progress < try_number_ * kMinimumExpectedProgressPerStep) {
    LoadOfflineReadingListEntry();
    StopCheckingProgress();
    return;
  }
  if (try_number_ >= 3) {
    // Loading reached 75%, let the page finish normal loading.
    // Do not call |StopCheckingProgress()| as |pending_url_| is still
    // needed to mark the entry read on success loading or to display
    // offline version on error.
    timer_->Stop();
  }
}

void ReadingListWebStateObserver::LoadOfflineReadingListEntry() {
  DCHECK(reading_list_model_);
  if (!pending_url_.is_valid() || !IsUrlAvailableOffline(pending_url_)) {
    return;
  }
  const ReadingListEntry* entry =
      reading_list_model_->GetEntryByURL(pending_url_);
  DCHECK(entry->DistilledState() == ReadingListEntry::PROCESSED);
  GURL url =
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL());
  web::NavigationManager* manager = web_state()->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();
  if (item) {
    web_state()->Stop();
    web::WebState::OpenURLParams params(url, item->GetReferrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        item->GetTransitionType(), NO);
    web_state()->OpenURL(params);
  } else {
    item = manager->GetLastCommittedItem();
    item->SetURL(url);
    item->SetVirtualURL(pending_url_);
    web_state()->GetNavigationManager()->Reload(false);
  }
  reading_list_model_->SetReadStatus(entry->URL(), true);
  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", true);
}
