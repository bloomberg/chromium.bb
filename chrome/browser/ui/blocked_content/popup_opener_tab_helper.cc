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
  if (visibility_tracker_after_tab_under_) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Tab.TabUnder.VisibleTime",
        visibility_tracker_after_tab_under_->GetForegroundDuration());
  }
  DCHECK(visibility_tracker_);
  UMA_HISTOGRAM_LONG_TIMES("Tab.VisibleTime",
                           visibility_tracker_->GetForegroundDuration());
}

void PopupOpenerTabHelper::OnOpenedPopup(PopupTracker* popup_tracker) {
  has_opened_popup_since_last_user_gesture_ = true;
  last_popup_open_time_ = tick_clock_->NowTicks();
}

void PopupOpenerTabHelper::OnDidTabUnder() {
  // The tab already did a tab-under.
  if (visibility_tracker_after_tab_under_)
    return;

  // Tab-under requires a popup, so this better not be null.
  DCHECK(!last_popup_open_time_.is_null());
  UMA_HISTOGRAM_LONG_TIMES("Tab.TabUnder.PopupToTabUnderTime",
                           tick_clock_->NowTicks() - last_popup_open_time_);

  // Careful, the tab can be in the foreground at this time! Current tab-under
  // heuristics use whether the navigation *started* in the background. We could
  // be getting this signal at redirect time, once we've realized that the
  // navigation is moving cross-origin.
  visibility_tracker_after_tab_under_ =
      base::MakeUnique<ScopedVisibilityTracker>(tick_clock_.get(),
                                                web_contents()->IsVisible());
}

PopupOpenerTabHelper::PopupOpenerTabHelper(
    content::WebContents* web_contents,
    std::unique_ptr<base::TickClock> tick_clock)
    : content::WebContentsObserver(web_contents),
      tick_clock_(std::move(tick_clock)) {
  visibility_tracker_ = base::MakeUnique<ScopedVisibilityTracker>(
      tick_clock_.get(), web_contents->IsVisible());
}

void PopupOpenerTabHelper::WasShown() {
  if (visibility_tracker_after_tab_under_)
    visibility_tracker_after_tab_under_->OnShown();
  visibility_tracker_->OnShown();
}

void PopupOpenerTabHelper::WasHidden() {
  if (visibility_tracker_after_tab_under_)
    visibility_tracker_after_tab_under_->OnHidden();
  visibility_tracker_->OnHidden();
}

void PopupOpenerTabHelper::DidGetUserInteraction(
    const blink::WebInputEvent::Type type) {
  has_opened_popup_since_last_user_gesture_ = false;
}
