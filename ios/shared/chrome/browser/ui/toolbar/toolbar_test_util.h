// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TEST_UTIL_H_
#define IOS_SHARED_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TEST_UTIL_H_

#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"

class ToolbarTestWebState : public web::TestWebState {
 public:
  ToolbarTestWebState();

  double GetLoadingProgress() const override;
  void set_loading_progress(double loading_progress);

 private:
  double loading_progress_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarTestWebState);
};

class ToolbarTestNavigationManager : public web::TestNavigationManager {
 public:
  ToolbarTestNavigationManager();

  bool CanGoBack() const override;
  bool CanGoForward() const override;

  void set_can_go_back(bool can_go_back);
  void set_can_go_forward(bool can_go_forward);

 private:
  bool can_go_back_;
  bool can_go_forward_;
};

#endif  // IOS_SHARED_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TEST_UTIL_H_
