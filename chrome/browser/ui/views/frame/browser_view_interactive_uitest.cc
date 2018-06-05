// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/scoped_macviews_browser_mode.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

using views::FocusManager;

namespace {

class BrowserViewTest : public InProcessBrowserTest {
 public:
  BrowserViewTest() = default;
  ~BrowserViewTest() override = default;

 private:
  test::ScopedMacViewsBrowserMode views_mode_{true};

  DISALLOW_COPY_AND_ASSIGN(BrowserViewTest);
};

}  // namespace

#if defined(OS_MACOSX)
// Encounters an internal MacOS assert: http://crbug.com/823490
#define MAYBE_FullscreenClearsFocus DISABLED_FullscreenClearsFocus
#else
#define MAYBE_FullscreenClearsFocus FullscreenClearsFocus
#endif
IN_PROC_BROWSER_TEST_F(BrowserViewTest, MAYBE_FullscreenClearsFocus) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());
  LocationBarView* location_bar_view = browser_view->GetLocationBarView();
  FocusManager* focus_manager = browser_view->GetFocusManager();

  // Focus starts in the location bar or one of its children.
  EXPECT_TRUE(location_bar_view->Contains(focus_manager->GetFocusedView()));

  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

  // Focus is released from the location bar.
  EXPECT_FALSE(location_bar_view->Contains(focus_manager->GetFocusedView()));
}

// Test whether the top view including toolbar and tab strip shows up or hides
// correctly in full screen mode.
IN_PROC_BROWSER_TEST_F(BrowserViewTest, FullscreenShowTopView) {
  BrowserView* browser_view = static_cast<BrowserView*>(browser()->window());

  // The top view should always show up in regular mode.
  EXPECT_FALSE(browser_view->IsFullscreen());
  EXPECT_TRUE(browser_view->IsTabStripVisible());

  // Enter into full screen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());

#if defined(OS_MACOSX)
  // The top view should show up by default.
  EXPECT_TRUE(browser_view->IsTabStripVisible());

  // Return back to normal mode and toggle to not show the top view in full
  // screen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view->IsFullscreen());
  chrome::ToggleFullscreenToolbar(browser());

  // While back to full screen mode, the top view no longer shows up.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view->IsFullscreen());
  EXPECT_FALSE(browser_view->IsTabStripVisible());
#else
  // In immersive full screen mode, the top view should show up; otherwise, it
  // always hides.
  if (browser_view->immersive_mode_controller()->IsEnabled())
    EXPECT_TRUE(browser_view->IsTabStripVisible());
  else
    EXPECT_FALSE(browser_view->IsTabStripVisible());
#endif
}
