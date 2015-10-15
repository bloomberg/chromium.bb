// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_view/navigation_controller.h"

#include "components/web_view/frame.h"
#include "components/web_view/navigation_controller_delegate.h"
#include "components/web_view/navigation_entry.h"
#include "components/web_view/reload_type.h"

namespace web_view {

NavigationController::NavigationController(
    NavigationControllerDelegate* delegate)
    : pending_entry_(nullptr),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      delegate_(delegate) {}

NavigationController::~NavigationController() {
  DiscardPendingEntry(false);
}

int NavigationController::GetCurrentEntryIndex() const {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

int NavigationController::GetIndexForOffset(int offset) const {
  return GetCurrentEntryIndex() + offset;
}

int NavigationController::GetEntryCount() const {
  // TODO(erg): Have a max number of entries, and DCHECK that we are smaller
  // than it here.
  return static_cast<int>(entries_.size());
}

NavigationEntry* NavigationController::GetEntryAtIndex(int index) const {
  if (index < 0 || index >= GetEntryCount())
    return nullptr;

  return entries_[index];
}

NavigationEntry* NavigationController::GetEntryAtOffset(int offset) const {
  return GetEntryAtIndex(GetIndexForOffset(offset));
}

bool NavigationController::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationController::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationController::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationController::GoBack() {
  if (!CanGoBack()) {
    NOTREACHED();
    return;
  }

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry(false);

  pending_entry_index_ = current_index - 1;
  // TODO(erg): Transition type handled here.
  NavigateToPendingEntry(ReloadType::NO_RELOAD, true);
}

void NavigationController::GoForward() {
  if (!CanGoForward()) {
    NOTREACHED();
    return;
  }

  // TODO(erg): The upstream version handles transience here.

  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  DiscardPendingEntry(false);

  pending_entry_index_ = current_index + 1;
  // TODO(erg): Transition type handled here.
  NavigateToPendingEntry(ReloadType::NO_RELOAD, true);
}

void NavigationController::LoadURL(mojo::URLRequestPtr request) {
  // TODO(erg): This mimics part of NavigationControllerImpl::LoadURL(), minus
  // all the error checking.
  SetPendingEntry(make_scoped_ptr(new NavigationEntry(request.Pass())));
  NavigateToPendingEntry(ReloadType::NO_RELOAD, false);
}

void NavigationController::NavigateToPendingEntry(
    ReloadType reload_type,
    bool update_navigation_start_time) {
  // TODO(erg): Deal with session history navigations while trying to navigate
  // to a slow-to-commit page.

  // TODO(erg): Deal with interstitials.

  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    CHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_];
  }

  // TODO(erg): Eventually, we need to deal with restoring the state of the
  // full tree. For now, we'll just shell back to the WebView.
  delegate_->OnNavigate(
      pending_entry_->BuildURLRequest(update_navigation_start_time));
}

void NavigationController::DiscardPendingEntry(bool was_failure) {
  // TODO(erg): We might copy the CHECK regarding NavigateToEntry here.

  // TODO(erg): We need to maintain the failed_pending_entry_ during
  // |was_failure| here.

  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = nullptr;
  pending_entry_index_ = -1;
}

void NavigationController::SetPendingEntry(scoped_ptr<NavigationEntry> entry) {
  // TODO(erg): Should be DiscardNonCommittedEntries() if we start porting
  // transient states over.
  DiscardPendingEntry(false);
  pending_entry_ = entry.release();
  DCHECK_EQ(-1, pending_entry_index_);
  // TODO(erg): The content code sends NOTIFICATION_NAV_ENTRY_PENDING here.
}

void NavigationController::FrameDidCommitProvisionalLoad(Frame* frame) {
  // Our renderer is committing a frame. If this was a real implementation,
  // we'd do something!
  if (frame->parent())
    return;

  // TODO(erg): Need some equivalent of InsertOrReplaceEntry(), as we need to
  // prune the history list.

  // TODO(erg): We should copy all the logic from the various
  // RendererDidNavigate* methods here.

  // TODO(erg): Medium term, we shouldn't be reusing the NavigationEntry, as it
  // appears that blink can change some of the data during the navigation. Do
  // it for now for bootstrapping purposes.
  if (pending_entry_index_ == -1 && pending_entry_) {
    ClearForwardEntries();
    entries_.push_back(pending_entry_);
    last_committed_entry_index_ = static_cast<int>(entries_.size() - 1);
    pending_entry_ = nullptr;
  } else if (pending_entry_index_ != -1 && pending_entry_) {
    last_committed_entry_index_ = pending_entry_index_;
    pending_entry_ = nullptr;

    // This was a historical navigation. In this limited prototype, we don't
    // actually do anything with it.
  } else {
    LOG(ERROR)
        << "Hit an unknown case in NavigationController::RendererDidNavigate";
  }

  DiscardPendingEntry(false);

  delegate_->OnDidNavigate();
}

void NavigationController::FrameDidNavigateLocally(Frame* frame,
                                                   const GURL& url) {
  // If this is a local navigation of a non-top frame, don't try to commit
  // it.
  if (frame->parent())
    return;

  ClearForwardEntries();

  // TODO(erg): This is overly cheap handling of local navigations in
  // frames. We don't have all the information needed to construct a real
  // URLRequest.

  entries_.push_back(new NavigationEntry(url));
  last_committed_entry_index_ = static_cast<int>(entries_.size() - 1);

  delegate_->OnDidNavigate();
}

void NavigationController::ClearForwardEntries() {
  int current_size = static_cast<int>(entries_.size());
  if (current_size > 0) {
    while (last_committed_entry_index_ < (current_size - 1)) {
      entries_.pop_back();
      current_size--;
    }
  }
}

}  // namespace web_view
