// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

class PopupTracker;
class ScopedVisibilityTracker;

// This class tracks WebContents for the purpose of logging metrics related to
// popup openers.
class PopupOpenerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupOpenerTabHelper> {
 public:
  ~PopupOpenerTabHelper() override;

  void set_tick_clock_for_testing(std::unique_ptr<base::TickClock> tick_clock) {
    tick_clock_ = std::move(tick_clock);
  }

  void OnOpenedPopup(PopupTracker* popup_tracker);

 private:
  friend class content::WebContentsUserData<PopupOpenerTabHelper>;

  explicit PopupOpenerTabHelper(content::WebContents* web_contents);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WasShown() override;
  void WasHidden() override;

  // Tracks navigations we are interested in, e.g. ones which start when the
  // WebContents is not visible.
  base::flat_set<content::NavigationHandle*> pending_background_navigations_;

  // The clock which gets passed to the |visibility_tracker_|.
  std::unique_ptr<base::TickClock> tick_clock_;

  // The |visibility_tracker| tracks the time this WebContents is in the
  // foreground. Will be nullptr until we redirect cross-origin in the
  // background.
  std::unique_ptr<ScopedVisibilityTracker> visibility_tracker_;

  // Measures the time this WebContents opened a popup before
  // |visibility_tracker_| is instantiated.
  base::TimeTicks last_popup_open_time_before_redirect_;

  DISALLOW_COPY_AND_ASSIGN(PopupOpenerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
