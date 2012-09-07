// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_

#include "chrome/browser/tab_contents/web_contents_user_data.h"
#include "content/public/browser/web_contents_observer.h"

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper : public content::WebContentsObserver,
                          public WebContentsUserData<MetroPinTabHelper> {
 public:
  explicit MetroPinTabHelper(content::WebContents* tab_contents);
  virtual ~MetroPinTabHelper();
  static int kUserDataKey;

  bool is_pinned() const { return is_pinned_; }

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Queries the metro driver about the pinned state of the current URL.
  void UpdatePinnedStateForCurrentURL();

  // Whether the current URL is pinned to the metro start screen.
  bool is_pinned_;

  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_
