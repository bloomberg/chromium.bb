// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/memory/scoped_ptr.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "grit/theme_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#endif

class BrowserViewTest : public BrowserWithTestWindowTest {
 public:
  BrowserViewTest();
  virtual ~BrowserViewTest() {}

  // BrowserWithTestWindowTest overrides:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;
  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE;

  BrowserView* browser_view() { return browser_view_; }

 private:
  BrowserView* browser_view_;  // Not owned.
  scoped_ptr<ScopedTestingLocalState> local_state_;
  DISALLOW_COPY_AND_ASSIGN(BrowserViewTest);
};

BrowserViewTest::BrowserViewTest()
    : browser_view_(NULL) {
}

void BrowserViewTest::SetUp() {
  local_state_.reset(
      new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManager);
#endif
  BrowserWithTestWindowTest::SetUp();
  browser_view_ = static_cast<BrowserView*>(browser()->window());
  // Memory ownership is tricky here. BrowserView has taken ownership of
  // |browser|, so BrowserWithTestWindowTest cannot continue to own it.
  ASSERT_TRUE(release_browser());
}

void BrowserViewTest::TearDown() {
  // Ensure the Browser is reset before BrowserWithTestWindowTest cleans up
  // the Profile.
  browser_view_->GetWidget()->CloseNow();
  browser_view_ = NULL;
  BrowserWithTestWindowTest::TearDown();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
  local_state_.reset(NULL);
}

BrowserWindow* BrowserViewTest::CreateBrowserWindow() {
  // Allow BrowserWithTestWindowTest to use Browser to create the default
  // BrowserView and BrowserFrame.
  return NULL;
}

// Test basic construction and initialization.
TEST_F(BrowserViewTest, BrowserView) {
  // The window is owned by the native widget, not the test class.
  EXPECT_FALSE(window());
  // |browser_view_| owns the Browser, not the test class.
  EXPECT_FALSE(browser());
  EXPECT_TRUE(browser_view()->browser());

  // Test initial state.
  EXPECT_TRUE(browser_view()->IsTabStripVisible());
  EXPECT_FALSE(browser_view()->IsOffTheRecord());
  EXPECT_EQ(IDR_OTR_ICON, browser_view()->GetOTRIconResourceID());
  EXPECT_FALSE(browser_view()->IsGuestSession());
  EXPECT_FALSE(browser_view()->ShouldShowAvatar());
  EXPECT_TRUE(browser_view()->IsBrowserTypeNormal());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_FALSE(browser_view()->IsBookmarkBarVisible());
  EXPECT_FALSE(browser_view()->IsBookmarkBarAnimating());

  // Ensure we've initialized enough to run Layout().
  browser_view()->Layout();
  // TDOO(jamescook): Layout assertions.
}
