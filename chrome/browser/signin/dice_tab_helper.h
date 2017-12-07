// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_
#define CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_

#include "base/macros.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// Tab helper used for DICE to mark that sync should start after a web sign-in
// with a Google account.
class DiceTabHelper : public content::WebContentsUserData<DiceTabHelper>,
                      public content::WebContentsObserver {
 public:
  ~DiceTabHelper() override;

  signin_metrics::AccessPoint signin_access_point() {
    return signin_access_point_;
  }

  signin_metrics::Reason signin_reason() { return signin_reason_; }

  // Returns true if sync should start after a Google web-signin flow.
  bool should_start_sync_after_web_signin() {
    return should_start_sync_after_web_signin_;
  }

  // Initializes the DiceTabHelper for a new signin flow. Must be called once
  // per signin flow happening in the tab.
  void InitializeSigninFlow(signin_metrics::AccessPoint access_point,
                            signin_metrics::Reason reason);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class content::WebContentsUserData<DiceTabHelper>;
  explicit DiceTabHelper(content::WebContents* web_contents);

  signin_metrics::AccessPoint signin_access_point_;
  signin_metrics::Reason signin_reason_;
  bool should_start_sync_after_web_signin_;

  DISALLOW_COPY_AND_ASSIGN(DiceTabHelper);
};

#endif  // CHROME_BROWSER_SIGNIN_DICE_TAB_HELPER_H_
