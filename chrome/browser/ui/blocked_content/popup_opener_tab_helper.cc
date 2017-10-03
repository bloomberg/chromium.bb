// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/ui/blocked_content/scoped_visibility_tracker.h"
#include "chrome/browser/ui/blocked_content/tab_under_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupOpenerTabHelper);

// static
void PopupOpenerTabHelper::CreateForWebContents(
    content::WebContents* contents,
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(contents);
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          base::WrapUnique(new PopupOpenerTabHelper(
                              contents, std::move(tick_clock))));
  }
}

PopupOpenerTabHelper::~PopupOpenerTabHelper() {
  if (visibility_tracker_after_redirect_) {
    base::TimeDelta foreground_duration =
        visibility_tracker_after_redirect_->GetForegroundDuration();
    UMA_HISTOGRAM_LONG_TIMES("Tab.VisibleTimeAfterCrossOriginRedirect",
                             foreground_duration);
    if (!last_popup_open_time_before_redirect_.is_null()) {
      UMA_HISTOGRAM_LONG_TIMES(
          "Tab.OpenedPopup.VisibleTimeAfterCrossOriginRedirect",
          foreground_duration);
    }
  }
  DCHECK(visibility_tracker_);
  UMA_HISTOGRAM_LONG_TIMES("Tab.VisibleTime",
                           visibility_tracker_->GetForegroundDuration());
}

void PopupOpenerTabHelper::OnOpenedPopup(PopupTracker* popup_tracker) {
  has_opened_popup_since_last_user_gesture_ = true;
  if (!visibility_tracker_after_redirect_)
    last_popup_open_time_before_redirect_ = tick_clock_->NowTicks();
}

PopupOpenerTabHelper::PopupOpenerTabHelper(
    content::WebContents* web_contents,
    std::unique_ptr<base::TickClock> tick_clock)
    : content::WebContentsObserver(web_contents),
      tick_clock_(std::move(tick_clock)) {
  visibility_tracker_ = base::MakeUnique<ScopedVisibilityTracker>(
      tick_clock_.get(), web_contents->IsVisible());
}

void PopupOpenerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (visibility_tracker_after_redirect_)
    return;

  if (navigation_handle->IsInMainFrame() && !web_contents()->IsVisible())
    pending_background_navigations_.insert(navigation_handle);
  // There should be at max 2 main frame navigations occurring at the same time.
  DCHECK_LE(pending_background_navigations_.size(), 2u);
}

void PopupOpenerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (visibility_tracker_after_redirect_ || !navigation_handle->IsInMainFrame())
    return;

  size_t num_erased = pending_background_navigations_.erase(navigation_handle);
  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
    return;

  if (!TabUnderNavigationThrottle::IsSuspiciousClientRedirect(
          navigation_handle, num_erased != 0 /* started_in_background */)) {
    return;
  }

  if (!last_popup_open_time_before_redirect_.is_null()) {
    // If long times doesn't have enough resolution, consider switching the
    // macro.
    UMA_HISTOGRAM_LONG_TIMES(
        "Tab.OpenedPopup.PopupToCrossOriginRedirectTime",
        tick_clock_->NowTicks() - last_popup_open_time_before_redirect_);
  }

  visibility_tracker_after_redirect_ =
      base::MakeUnique<ScopedVisibilityTracker>(tick_clock_.get(),
                                                false /* is_visible */);
  pending_background_navigations_.clear();
}

void PopupOpenerTabHelper::WasShown() {
  if (visibility_tracker_after_redirect_)
    visibility_tracker_after_redirect_->OnShown();
  visibility_tracker_->OnShown();
}

void PopupOpenerTabHelper::WasHidden() {
  if (visibility_tracker_after_redirect_)
    visibility_tracker_after_redirect_->OnHidden();
  visibility_tracker_->OnHidden();
}

void PopupOpenerTabHelper::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  has_opened_popup_since_last_user_gesture_ = false;
}
