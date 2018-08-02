// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_IDN_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_UI_OMNIBOX_IDN_NAVIGATION_OBSERVER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Observes navigations and shows an infobar if an IDN hostname visually looks
// like a top domain.
class IdnNavigationObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<IdnNavigationObserver> {
 public:
  // Used for metrics.
  enum class NavigationSuggestionEvent {
    kNone = 0,
    kInfobarShown = 1,
    kLinkClicked = 2,

    // Append new items to the end of the list above; do not modify or
    // replace existing values. Comment out obsolete items.
    kMaxValue = kLinkClicked,
  };

  static const char kHistogramName[];

  static void CreateForWebContents(content::WebContents* web_contents);

  explicit IdnNavigationObserver(content::WebContents* web_contents);
  ~IdnNavigationObserver() override;

  // content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_IDN_NAVIGATION_OBSERVER_H_
