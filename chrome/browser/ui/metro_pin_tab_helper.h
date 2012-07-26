// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_
#define CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_

#include "content/public/browser/web_contents_observer.h"

class MetroPinnedStateObserver;

// Per-tab class to help manage metro pinning.
class MetroPinTabHelper : public content::WebContentsObserver {
 public:
  explicit MetroPinTabHelper(content::WebContents* web_contents);
  virtual ~MetroPinTabHelper();

  bool is_pinned() const { return is_pinned_; }

  void set_observer(MetroPinnedStateObserver* observer) {
    observer_ = observer;
  }

  void TogglePinnedToStartScreen();

  // content::WebContentsObserver overrides:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  // Queries the metro driver about the pinned state of the current URL.
  void UpdatePinnedStateForCurrentURL();

  // Update the pinned state and notify the delegate.
  void SetIsPinned(bool is_pinned);

  // Whether the current URL is pinned to the metro start screen.
  bool is_pinned_;

  // The observer that we inform when the |is_pinned_| state changes.
  MetroPinnedStateObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(MetroPinTabHelper);
};

#endif  // CHROME_BROWSER_UI_METRO_PIN_TAB_HELPER_H_
