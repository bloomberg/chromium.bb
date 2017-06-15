// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_container_view.h"

#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Fixture for BrowserContainerView testing.
class BrowserContainerViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    browser_container_view_ = [[BrowserContainerView alloc] init];
    ASSERT_TRUE(browser_container_view_);
    content_view_ = [[UIView alloc] init];
    ASSERT_TRUE(content_view_);
  }
  BrowserContainerView* browser_container_view_;
  UIView* content_view_;
};

// Tests adding a new content view when BrowserContainerView does not currently
// have a content view.
TEST_F(BrowserContainerViewTest, AddingContentView) {
  ASSERT_FALSE([content_view_ superview]);

  [browser_container_view_ displayContentView:content_view_];
  EXPECT_EQ(static_cast<UIView*>(browser_container_view_),
            [content_view_ superview]);
}

// Tests removing previously added content view.
TEST_F(BrowserContainerViewTest, RemovingContentView) {
  [browser_container_view_ displayContentView:content_view_];
  ASSERT_EQ(static_cast<UIView*>(browser_container_view_),
            [content_view_ superview]);

  [browser_container_view_ displayContentView:nil];
  EXPECT_FALSE([content_view_ superview]);
}

// Tests adding a new content view when BrowserContainerView already has a
// content view.
TEST_F(BrowserContainerViewTest, ReplacingContentView) {
  [browser_container_view_ displayContentView:content_view_];
  ASSERT_EQ(static_cast<UIView*>(browser_container_view_),
            [content_view_ superview]);

  UIView* content_view2 = [[UIView alloc] init];
  [browser_container_view_ displayContentView:content_view2];
  EXPECT_FALSE([content_view_ superview]);
  EXPECT_EQ(static_cast<UIView*>(browser_container_view_),
            [content_view2 superview]);
}
