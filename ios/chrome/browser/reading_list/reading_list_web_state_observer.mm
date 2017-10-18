// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"

#import <Foundation/Foundation.h>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/reload_type.h"
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
      web_state->SetUserData(kObserverKey, std::move(newDataWrapper));
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
      last_load_was_offline_(false),
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
    DidStartLoading(web_state());
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

void ReadingListWebStateObserver::DidStartLoading(web::WebState* web_state) {
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
  bool is_reload = false;

  // Manager->GetPendingItem() returns null on reload.
  // TODO(crbug.com/676129): Remove this workaround once GetPendingItem()
  // returns the correct value on reload.
  if (!item) {
    item = manager->GetLastCommittedItem();
    is_reload = true;
  }

  if (!ShouldObserveItem(item)) {
    StopCheckingProgress();
    return;
  }
  bool last_load_was_offline = last_load_was_offline_;
  last_load_was_offline_ = false;

  pending_url_ = item->GetVirtualURL();

  is_reload =
      is_reload || ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                                ui::PAGE_TRANSITION_RELOAD);
  // If the user is reloading from the offline page, the intention is to access
  // the online page even on bad networks. No need to launch timer.
  bool reloading_from_offline = last_load_was_offline && is_reload;

  // No need to launch the timer either if there is no offline version to show.
  // Track |pending_url_| to mark the entry as read in case of a successful
  // load.
  if (reloading_from_offline || !IsUrlAvailableOffline(pending_url_)) {
    return;
  }
  try_number_ = 0;
  timer_.reset(new base::Timer(false, true));
  const base::TimeDelta kDelayUntilLoadingProgressIsChecked =
      base::TimeDelta::FromMilliseconds(1500);
  timer_->Start(
      FROM_HERE, kDelayUntilLoadingProgressIsChecked,
      base::Bind(
          &ReadingListWebStateObserver::VerifyIfReadingListEntryStartedLoading,
          base::Unretained(this)));
}

void ReadingListWebStateObserver::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK(reading_list_model_);
  DCHECK(web_state);
  last_load_result_ = load_completion_status;
  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetLastCommittedItem();
  if (!item || !pending_url_.is_valid() ||
      !reading_list_model_->GetEntryByURL(pending_url_)) {
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

void ReadingListWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  StopCheckingProgress();
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  web_state->RemoveUserData(kObserverKey);
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
  last_load_was_offline_ = true;
  DCHECK(entry->DistilledState() == ReadingListEntry::PROCESSED);
  GURL url = reading_list::OfflineURLForPath(
      entry->DistilledPath(), entry->URL(), entry->DistilledURL());
  web::NavigationManager* navigationManager =
      web_state()->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetPendingItem();
  if (!item) {
    // Either the loading finished on error and the item is already committed,
    // or the page is being reloaded and due to crbug.com/676129. there is no
    // pending item. Either way, the correct item to reuse is the last committed
    // item.
    // TODO(crbug.com/676129): this case can be removed.
    item = navigationManager->GetLastCommittedItem();
    item->SetURL(url);
    item->SetVirtualURL(pending_url_);
    // It is not possible to call |navigationManager->Reload| at that point as
    // an error page is currently displayed.
    // Calling Reload will eventually call |ErrorPageContent reload| which
    // simply loads |pending_url_| and will erase the distilled_url.
    // Instead, go to the index that will branch further in the reload stack
    // and avoid this situation.
    navigationManager->GoToIndex(
        navigationManager->GetLastCommittedItemIndex());
  } else if (navigationManager->GetPendingItemIndex() != -1 &&
             navigationManager->GetItemAtIndex(
                 navigationManager->GetPendingItemIndex()) == item) {
    // The navigation corresponds to a back/forward. The item on the stack can
    // be reused for the offline navigation.
    // TODO(crbug.com/665189): GetPendingItemIndex() will return currentEntry()
    // if navigating to a new URL. Test the addresses to verify that
    // GetPendingItemIndex() actually returns the pending item index. Remove
    // this extra test on item addresses once bug 665189 is fixed.
    item->SetURL(url);
    item->SetVirtualURL(pending_url_);
    navigationManager->GoToIndex(navigationManager->GetPendingItemIndex());
  } else {
    // The pending item corresponds to a new navigation and will be discarded
    // on next navigation.
    // Trigger a new navigation on the offline URL.
    web::WebState::OpenURLParams params(url, item->GetReferrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        item->GetTransitionType(), NO);
    web_state()->OpenURL(params);
  }
  reading_list_model_->SetReadStatus(entry->URL(), true);
  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", true);
}
