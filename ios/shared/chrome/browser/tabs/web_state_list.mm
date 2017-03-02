// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_order_controller.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Wrapper around a WebState stored in a WebStateList.
class WebStateList::WebStateWrapper {
 public:
  explicit WebStateWrapper(web::WebState* web_state);
  ~WebStateWrapper();

  web::WebState* web_state() const { return web_state_; }
  web::WebState* opener() const { return opener_; }

  // Replaces the wrapped WebState (and clear associated state) and returns the
  // old WebState after forfeiting ownership.
  web::WebState* ReplaceWebState(web::WebState* web_state);

  // Sets the opener for the wrapped WebState and record the opener navigation
  // index to allow detecting navigation changes during the same session.
  void SetOpener(web::WebState* opener);

  // Returns whether |opener| spawned the wrapped WebState. If |use_group| is
  // true, also use the opener navigation index to detect navigation changes
  // during the same session.
  bool WasOpenedBy(const web::WebState* opener,
                   int opener_navigation_index,
                   bool use_group) const;

 private:
  web::WebState* web_state_;
  web::WebState* opener_ = nullptr;
  int opener_last_committed_index_;

  DISALLOW_COPY_AND_ASSIGN(WebStateWrapper);
};

WebStateList::WebStateWrapper::WebStateWrapper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
}

WebStateList::WebStateWrapper::~WebStateWrapper() = default;

web::WebState* WebStateList::WebStateWrapper::ReplaceWebState(
    web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_NE(web_state, web_state_);
  std::swap(web_state, web_state_);
  opener_ = nullptr;
  return web_state;
}

void WebStateList::WebStateWrapper::SetOpener(web::WebState* opener) {
  opener_ = opener;
  if (opener_) {
    opener_last_committed_index_ =
        opener_->GetNavigationManager()->GetLastCommittedItemIndex();
  }
}

bool WebStateList::WebStateWrapper::WasOpenedBy(const web::WebState* opener,
                                                int opener_navigation_index,
                                                bool use_group) const {
  DCHECK(opener);
  if (opener_ != opener)
    return false;

  if (!use_group)
    return true;

  return opener_last_committed_index_ == opener_navigation_index;
}

WebStateList::WebStateList(WebStateOwnership ownership)
    : web_state_ownership_(ownership),
      order_controller_(base::MakeUnique<WebStateListOrderController>(this)) {}

WebStateList::~WebStateList() {
  // Once WebStateList owns the WebState and has a CloseWebStateAt() method,
  // then change this to close all the WebState. See http://crbug.com/546222
  // for progress.
  if (web_state_ownership_ == WebStateOwned) {
    for (auto& web_state_wrapper : web_state_wrappers_) {
      web::WebState* web_state = web_state_wrapper->web_state();
      delete web_state;
    }
  }
}

bool WebStateList::ContainsIndex(int index) const {
  return 0 <= index && index < count();
}

web::WebState* WebStateList::GetActiveWebState() const {
  if (active_index_ != kInvalidIndex)
    return GetWebStateAt(active_index_);
  return nullptr;
}

web::WebState* WebStateList::GetWebStateAt(int index) const {
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->web_state();
}

int WebStateList::GetIndexOfWebState(const web::WebState* web_state) const {
  for (int index = 0; index < count(); ++index) {
    if (web_state_wrappers_[index]->web_state() == web_state)
      return index;
  }
  return kInvalidIndex;
}

web::WebState* WebStateList::GetOpenerOfWebStateAt(int index) const {
  DCHECK(ContainsIndex(index));
  return web_state_wrappers_[index]->opener();
}

void WebStateList::SetOpenerOfWebStateAt(int index, web::WebState* opener) {
  DCHECK(ContainsIndex(index));
  DCHECK(ContainsIndex(GetIndexOfWebState(opener)));
  web_state_wrappers_[index]->SetOpener(opener);
}

int WebStateList::GetIndexOfNextWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, 1);
}

int WebStateList::GetIndexOfLastWebStateOpenedBy(const web::WebState* opener,
                                                 int start_index,
                                                 bool use_group) const {
  return GetIndexOfNthWebStateOpenedBy(opener, start_index, use_group, INT_MAX);
}

void WebStateList::InsertWebState(int index,
                                  web::WebState* web_state,
                                  web::WebState* opener) {
  DCHECK(ContainsIndex(index) || index == count());
  web_state_wrappers_.insert(web_state_wrappers_.begin() + index,
                             base::MakeUnique<WebStateWrapper>(web_state));

  if (active_index_ >= index)
    ++active_index_;

  if (opener)
    SetOpenerOfWebStateAt(index, opener);

  for (auto& observer : observers_)
    observer.WebStateInsertedAt(this, web_state, index);
}

void WebStateList::AppendWebState(ui::PageTransition transition,
                                  web::WebState* web_state,
                                  web::WebState* opener) {
  int index = order_controller_->DetermineInsertionIndex(transition, opener);
  if (index < 0 || count() < index)
    index = count();

  InsertWebState(index, web_state, opener);
}

void WebStateList::MoveWebStateAt(int from_index, int to_index) {
  DCHECK(ContainsIndex(from_index));
  DCHECK(ContainsIndex(to_index));
  if (from_index == to_index)
    return;

  std::unique_ptr<WebStateWrapper> web_state_wrapper =
      std::move(web_state_wrappers_[from_index]);
  web::WebState* web_state = web_state_wrapper->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + from_index);
  web_state_wrappers_.insert(web_state_wrappers_.begin() + to_index,
                             std::move(web_state_wrapper));

  if (active_index_ == from_index) {
    active_index_ = to_index;
  } else {
    int min = std::min(from_index, to_index);
    int max = std::max(from_index, to_index);
    int delta = from_index < to_index ? -1 : +1;
    if (min <= active_index_ && active_index_ <= max)
      active_index_ += delta;
  }

  for (auto& observer : observers_)
    observer.WebStateMoved(this, web_state, from_index, to_index);
}

web::WebState* WebStateList::ReplaceWebStateAt(int index,
                                               web::WebState* web_state,
                                               web::WebState* opener) {
  DCHECK(ContainsIndex(index));
  ClearOpenersReferencing(index);

  auto& web_state_wrapper = web_state_wrappers_[index];
  web::WebState* old_web_state = web_state_wrapper->ReplaceWebState(web_state);

  if (opener && opener != old_web_state)
    SetOpenerOfWebStateAt(index, opener);

  for (auto& observer : observers_)
    observer.WebStateReplacedAt(this, old_web_state, web_state, index);

  return old_web_state;
}

web::WebState* WebStateList::DetachWebStateAt(int index) {
  DCHECK(ContainsIndex(index));
  ClearOpenersReferencing(index);

  int new_active_index = order_controller_->DetermineNewActiveIndex(index);

  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  web_state_wrappers_.erase(web_state_wrappers_.begin() + index);

  // Update the active index to prevent observer from seeing an invalid WebState
  // as the active one but only send the WebStateActivatedAt notification after
  // the WebStateDetachedAt one.
  bool active_web_state_was_closed = (index == active_index_);
  if (active_index_ > index)
    --active_index_;
  else if (active_index_ == index)
    active_index_ = new_active_index;

  for (auto& observer : observers_)
    observer.WebStateDetachedAt(this, old_web_state, index);

  if (active_web_state_was_closed)
    NotifyIfActiveWebStateChanged(old_web_state, false);

  return old_web_state;
}

void WebStateList::ActivateWebStateAt(int index) {
  DCHECK(ContainsIndex(index));
  web::WebState* old_web_state = GetActiveWebState();
  active_index_ = index;
  NotifyIfActiveWebStateChanged(old_web_state, true);
}

void WebStateList::AddObserver(WebStateListObserver* observer) {
  observers_.AddObserver(observer);
}

void WebStateList::RemoveObserver(WebStateListObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebStateList::ClearOpenersReferencing(int index) {
  web::WebState* old_web_state = web_state_wrappers_[index]->web_state();
  for (auto& web_state_wrapper : web_state_wrappers_) {
    if (web_state_wrapper->opener() == old_web_state)
      web_state_wrapper->SetOpener(nullptr);
  }
}

void WebStateList::NotifyIfActiveWebStateChanged(web::WebState* old_web_state,
                                                 bool user_action) {
  web::WebState* new_web_state = GetActiveWebState();
  if (old_web_state == new_web_state)
    return;

  for (auto& observer : observers_) {
    observer.WebStateActivatedAt(this, old_web_state, new_web_state,
                                 active_index_, user_action);
  }
}

int WebStateList::GetIndexOfNthWebStateOpenedBy(const web::WebState* opener,
                                                int start_index,
                                                bool use_group,
                                                int n) const {
  DCHECK_GT(n, 0);
  if (!opener || !ContainsIndex(start_index) || start_index == INT_MAX)
    return kInvalidIndex;

  const int opener_navigation_index =
      use_group ? opener->GetNavigationManager()->GetCurrentItemIndex() : -1;

  int found_index = kInvalidIndex;
  for (int index = start_index + 1; index < count() && n; ++index) {
    if (web_state_wrappers_[index]->WasOpenedBy(opener, opener_navigation_index,
                                                use_group)) {
      found_index = index;
      --n;
    } else if (found_index != kInvalidIndex) {
      return found_index;
    }
  }

  return found_index;
}

// static
const int WebStateList::kInvalidIndex;
