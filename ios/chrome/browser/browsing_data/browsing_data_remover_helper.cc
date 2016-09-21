// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/browsing_data/browsing_data_remover_helper.h"

#include <utility>

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

BrowsingDataRemoverHelper::BrowsingDataRemoverHelper()
    : current_remover_(nullptr) {}

BrowsingDataRemoverHelper::~BrowsingDataRemoverHelper() {
  DCHECK(pending_removals_.empty());
}

BrowsingDataRemoverHelper::BrowsingDataRemovalInfo::BrowsingDataRemovalInfo(
    int remove_mask,
    browsing_data::TimePeriod time_period,
    const base::Closure& callback)
    : remove_mask(remove_mask), time_period(time_period) {
  callbacks.push_back(callback);
}

BrowsingDataRemoverHelper::BrowsingDataRemovalInfo::~BrowsingDataRemovalInfo() {
}

void BrowsingDataRemoverHelper::Remove(ios::ChromeBrowserState* browser_state,
                                       int remove_mask,
                                       browsing_data::TimePeriod time_period,
                                       const base::Closure& callback) {
  DCHECK(browser_state);
  DCHECK(!browser_state->IsOffTheRecord());
  // IOSChromeBrowsingDataRemover::Callbacks are called before
  // OnIOSChromeBrowsingDataRemoverDone() and after
  // IOSChromeBrowsingDataRemover::is_removing() is set to false. In this
  // window, |current_remover_| needs to be checked as well.
  if (current_remover_ || IOSChromeBrowsingDataRemover::is_removing()) {
    // IOSChromeBrowsingDataRemover is not re-entrant. If it is already running,
    // add the browser_state to |pending_removals_| for later deletion. If the
    // browser_state is already scheduled for removal of browsing data, update
    // the remove mask and callbacks.
    DCHECK(current_remover_);
    auto pending_removals_iter = pending_removals_.find(browser_state);
    if (pending_removals_iter == pending_removals_.end()) {
      std::unique_ptr<BrowsingDataRemovalInfo> removal_info(
          new BrowsingDataRemovalInfo(remove_mask, time_period, callback));
      pending_removals_[browser_state] = std::move(removal_info);
    } else {
      pending_removals_iter->second->remove_mask |= remove_mask;
      pending_removals_iter->second->callbacks.push_back(callback);
    }
  } else {
    std::unique_ptr<BrowsingDataRemovalInfo> removal_info(
        new BrowsingDataRemovalInfo(remove_mask, time_period, callback));
    DoRemove(browser_state, std::move(removal_info));
  }
}

void BrowsingDataRemoverHelper::OnIOSChromeBrowsingDataRemoverDone() {
  current_remover_ = nullptr;

  DCHECK(current_removal_info_);
  // Inform clients of the currently finished removal operation that browsing
  // data was removed.
  for (const auto& callback : current_removal_info_->callbacks) {
    if (!callback.is_null()) {
      callback.Run();
    }
  }
  current_removal_info_.reset();

  if (pending_removals_.empty())
    return;

  ios::ChromeBrowserState* next_browser_state =
      pending_removals_.begin()->first;
  std::unique_ptr<BrowsingDataRemovalInfo> removal_info =
      std::move(pending_removals_[next_browser_state]);
  pending_removals_.erase(next_browser_state);
  DoRemove(next_browser_state, std::move(removal_info));
}

void BrowsingDataRemoverHelper::DoRemove(
    ios::ChromeBrowserState* browser_state,
    std::unique_ptr<BrowsingDataRemovalInfo> removal_info) {
  DCHECK(!current_remover_ && !IOSChromeBrowsingDataRemover::is_removing());

  current_removal_info_ = std::move(removal_info);

  // IOSChromeBrowsingDataRemover deletes itself.
  IOSChromeBrowsingDataRemover* remover =
      IOSChromeBrowsingDataRemover::CreateForPeriod(
          browser_state, current_removal_info_->time_period);
  remover->AddObserver(this);
  current_remover_ = remover;
  int remove_mask = current_removal_info_->remove_mask;
  remover->Remove(remove_mask);
}
