// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/popup_tracker.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/ui/blocked_content/popup_opener_tab_helper.h"
#include "chrome/browser/ui/blocked_content/scoped_visibility_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(PopupTracker);

void PopupTracker::CreateForWebContents(content::WebContents* contents,
                                        content::WebContents* opener) {
  DCHECK(contents);
  DCHECK(opener);
  if (!FromWebContents(contents)) {
    contents->SetUserData(UserDataKey(),
                          base::WrapUnique(new PopupTracker(contents, opener)));
  }
}

PopupTracker::~PopupTracker() {
  if (first_load_visibility_tracker_) {
    UMA_HISTOGRAM_LONG_TIMES(
        "ContentSettings.Popups.FirstDocumentEngagementTime2",
        first_load_visibility_tracker_->GetForegroundDuration());
  }
}

PopupTracker::PopupTracker(content::WebContents* contents,
                           content::WebContents* opener)
    : content::WebContentsObserver(contents) {
  if (auto* popup_opener = PopupOpenerTabHelper::FromWebContents(opener))
    popup_opener->OnOpenedPopup(this);
}

void PopupTracker::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // The existence of |first_load_visibility_tracker_| is a proxy for whether
  // we've committed the first navigation in this WebContents.
  if (!first_load_visibility_tracker_) {
    first_load_visibility_tracker_ = base::MakeUnique<ScopedVisibilityTracker>(
        base::MakeUnique<base::DefaultTickClock>(),
        web_contents()->IsVisible());
  } else {
    web_contents()->RemoveUserData(UserDataKey());
    // Destroys this object.
  }
}

void PopupTracker::WasShown() {
  if (first_load_visibility_tracker_)
    first_load_visibility_tracker_->OnShown();
}

void PopupTracker::WasHidden() {
  if (first_load_visibility_tracker_)
    first_load_visibility_tracker_->OnHidden();
}
