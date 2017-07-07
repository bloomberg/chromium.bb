// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_TAB_TAB_TEST_UTIL_H_
#define IOS_SHARED_CHROME_BROWSER_UI_TAB_TAB_TEST_UTIL_H_

#import "ios/web/public/test/fakes/test_navigation_manager.h"

class TabNavigationManager : public web::TestNavigationManager {
 public:
  int GetItemCount() const override;
  bool CanGoForward() const override;
  bool CanGoBack() const override;

  void LoadURLWithParams(const NavigationManager::WebLoadParams&) override;
  void SetItemCount(int count);
  bool GetHasLoadedUrl();

 private:
  int item_count_;
  bool has_loaded_url_;
};

#endif  // IOS_SHARED_CHROME_BROWSER_UI_TAB_TAB_TEST_UTIL_H_
