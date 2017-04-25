// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/test_toolbar_model_ios.h"

#include "components/toolbar/toolbar_model_impl.h"
#import "ios/chrome/browser/tabs/tab.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestToolbarModelIOS::TestToolbarModelIOS()
    : ToolbarModelIOS(),
      is_loading_(false),
      load_progress_fraction_(0.0),
      can_go_back_(false),
      can_go_forward_(false),
      is_current_tab_native_page_(false),
      is_current_tab_bookmarked_(false) {
  test_toolbar_model_.reset(new TestToolbarModel());
}

TestToolbarModelIOS::~TestToolbarModelIOS() {}

ToolbarModel* TestToolbarModelIOS::GetToolbarModel() {
  return test_toolbar_model_.get();
}

bool TestToolbarModelIOS::IsLoading() {
  return is_loading_;
}

CGFloat TestToolbarModelIOS::GetLoadProgressFraction() {
  return load_progress_fraction_;
}

bool TestToolbarModelIOS::CanGoBack() {
  return can_go_back_;
}

bool TestToolbarModelIOS::CanGoForward() {
  return can_go_forward_;
}

bool TestToolbarModelIOS::IsCurrentTabNativePage() {
  return is_current_tab_native_page_;
}

bool TestToolbarModelIOS::IsCurrentTabBookmarked() {
  return is_current_tab_bookmarked_;
}

bool TestToolbarModelIOS::IsCurrentTabBookmarkedByUser() {
  return is_current_tab_bookmarked_;
}

bool TestToolbarModelIOS::ShouldDisplayHintText() {
  return false;
}

