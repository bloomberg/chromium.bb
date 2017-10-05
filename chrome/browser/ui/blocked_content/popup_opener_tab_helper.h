// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace base {
class TickClock;
}

namespace content {
class WebContents;
}

class PopupTracker;
class ScopedVisibilityTracker;

// This class tracks WebContents for the purpose of logging metrics related to
// popup openers.
class PopupOpenerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PopupOpenerTabHelper> {
 public:
  static void CreateForWebContents(content::WebContents* contents,
                                   std::unique_ptr<base::TickClock> tick_clock);
  ~PopupOpenerTabHelper() override;

  void OnOpenedPopup(PopupTracker* popup_tracker);
  void OnDidTabUnder();

  bool has_opened_popup_since_last_user_gesture() const {
    return has_opened_popup_since_last_user_gesture_;
  }

 private:
  friend class content::WebContentsUserData<PopupOpenerTabHelper>;

  PopupOpenerTabHelper(content::WebContents* web_contents,
                       std::unique_ptr<base::TickClock> tick_clock);

  // content::WebContentsObserver:
  void WasShown() override;
  void WasHidden() override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // The clock which is used by the visibility trackers.
  std::unique_ptr<base::TickClock> tick_clock_;

  // The |visibility_tracker_after_tab_under_| tracks the time this WebContents
  // is in the foreground, after the tab does a cross site redirect in the
  // background. Will be nullptr until that point in time.
  std::unique_ptr<ScopedVisibilityTracker> visibility_tracker_after_tab_under_;

  // Keeps track of the total foreground time for this tab.
  std::unique_ptr<ScopedVisibilityTracker> visibility_tracker_;

  // Measures the time this WebContents opened a popup.
  base::TimeTicks last_popup_open_time_;

  bool has_opened_popup_since_last_user_gesture_ = false;

  DISALLOW_COPY_AND_ASSIGN(PopupOpenerTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_OPENER_TAB_HELPER_H_
