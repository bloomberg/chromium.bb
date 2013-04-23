// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

// Subclass BrowserView so we can break its dependency on having a BrowserFrame.
class TestBrowserView : public BrowserView {
 public:
  TestBrowserView() {}
  virtual ~TestBrowserView() {}

  // Overridden from BrowserWindow:
  virtual bool IsFullscreen() const OVERRIDE { return false; }
};

/////////////////////////////////////////////////////////////////////////////

class BrowserViewTest : public BrowserWithTestWindowTest {
 public:
  BrowserViewTest() : browser_view_(NULL) {}
  virtual ~BrowserViewTest() {}

  virtual void SetUp() OVERRIDE {
    // BrowserWithTestWindowTest takes ownership of BrowserView via |window_|.
    browser_view_ = new TestBrowserView;
    set_window(browser_view_);

    // Creates a Browser using |browser_view| as its BrowserWindow.
    BrowserWithTestWindowTest::SetUp();

    // Memory ownership is tricky here. BrowserView needs ownership of
    // |browser|, so BrowserWithTestWindowTest cannot continue to own it.
    Browser* browser = release_browser();
    browser_view_->Init(browser);
  }

  BrowserView* browser_view() { return browser_view_; }

 private:
  BrowserView* browser_view_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(BrowserViewTest);
};

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // |browser_view_| owns the Browser, not the test class.
  EXPECT_FALSE(browser());
  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  EXPECT_FALSE(browser_view()->IsOffTheRecord());
  EXPECT_EQ(IDR_OTR_ICON, browser_view()->GetOTRIconResourceID());
  EXPECT_FALSE(browser_view()->IsGuestSession());
  EXPECT_TRUE(browser_view()->IsBrowserTypeNormal());

  // TODO(jamescook): Add additional tests. Instantiating a ProfileManager
  // would allow testing of avatar icons.
}
