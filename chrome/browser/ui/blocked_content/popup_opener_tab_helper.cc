// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/blocked_content/scoped_visibility_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupOpenerTabHelper);

PopupOpenerTabHelper::~PopupOpenerTabHelper() {
  // TODO(csharrison): Add breakout metrics for when this WebContents has opened
  // a popup in its lifetime.
  if (visibility_tracker_) {
    UMA_HISTOGRAM_LONG_TIMES("Tab.VisibleTimeAfterCrossOriginRedirect",
                             visibility_tracker_->GetForegroundDuration());
  }
}

PopupOpenerTabHelper::PopupOpenerTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      tick_clock_(base::MakeUnique<base::DefaultTickClock>()) {}

void PopupOpenerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (visibility_tracker_)
    return;

  if (navigation_handle->IsInMainFrame() && !web_contents()->IsVisible())
    pending_background_navigations_.insert(navigation_handle);
  // There should be at max 2 main frame navigations occurring at the same time.
  DCHECK_LE(pending_background_navigations_.size(), 2u);
}

void PopupOpenerTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (visibility_tracker_ || !navigation_handle->IsInMainFrame())
    return;

  size_t num_erased = pending_background_navigations_.erase(navigation_handle);
  if (!num_erased || !navigation_handle->HasCommitted() ||
      navigation_handle->IsErrorPage()) {
    return;
  }

  // Only consider navigations without a user gesture.
  if (navigation_handle->HasUserGesture())
    return;

  // An empty previous URL indicates this was the first load. We filter these
  // out because we're primarily interested in sites which navigate themselves
  // away while in the background.
  const GURL& previous_main_frame_url = navigation_handle->GetPreviousURL();
  if (previous_main_frame_url.is_empty())
    return;

  // Only track cross-origin navigations.
  if (url::Origin(previous_main_frame_url)
          .IsSameOriginWith(url::Origin(navigation_handle->GetURL()))) {
    return;
  }

  visibility_tracker_ = base::MakeUnique<ScopedVisibilityTracker>(
      std::move(tick_clock_), false /* is_visible */);
  pending_background_navigations_.clear();
}

void PopupOpenerTabHelper::WasShown() {
  if (visibility_tracker_)
    visibility_tracker_->OnShown();
}

void PopupOpenerTabHelper::WasHidden() {
  if (visibility_tracker_)
    visibility_tracker_->OnHidden();
}
