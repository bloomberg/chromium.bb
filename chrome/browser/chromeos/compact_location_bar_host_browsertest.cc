// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/compact_location_bar_host.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

class CompactLocationBarHostTest : public InProcessBrowserTest {
 public:
  CompactLocationBarHostTest() {
  }

  virtual void SetUp() {
    // Don't animate during the test.
    DropdownBarHost::disable_animations_during_testing_ = true;
    InProcessBrowserTest::SetUp();
  }

  BrowserView* browser_view() const {
    return static_cast<BrowserView*>(browser()->window());
  }

  gfx::NativeWindow window() const {
    return browser_view()->GetNativeHandle();
  }

  CompactLocationBarHost* clb_host() const {
    return browser_view()->compact_location_bar_host();
  }

  bool IsViewIdVisible(int id) const {
    return browser_view()->GetViewByID(id)->IsVisible();
  }

  bool IsCurrentTabIndex(int index) {
    return clb_host()->IsCurrentTabIndex(index);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactLocationBarHostTest);
};

IN_PROC_BROWSER_TEST_F(CompactLocationBarHostTest, TestCtrlLOpen) {
  // ctrl-l should not open compact location bar in normal mode.
  ui_controls::SendKeyPress(window(), base::VKEY_L, true, false, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(-1));
  EXPECT_FALSE(clb_host()->IsVisible());

  browser()->ToggleCompactNavigationBar();
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(-1));
  EXPECT_FALSE(clb_host()->IsVisible());

  // ctrl-l should not open compact location bar in compact nav mode.
  ui_controls::SendKeyPress(window(), base::VKEY_L, true, false, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(0));
  EXPECT_TRUE(clb_host()->IsVisible());

  // Esc to close it.
  ui_controls::SendKeyPress(window(), base::VKEY_ESCAPE, false, false, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(0));
  EXPECT_FALSE(clb_host()->IsVisible());
}

IN_PROC_BROWSER_TEST_F(CompactLocationBarHostTest, TestOnNewTab) {
  browser()->ToggleCompactNavigationBar();
  ui_controls::SendKeyPress(window(), base::VKEY_L, true, false, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(0));
  EXPECT_TRUE(clb_host()->IsVisible());

  browser()->NewTab();
  RunAllPendingEvents();
  EXPECT_FALSE(clb_host()->IsVisible());

  ui_controls::SendKeyPress(window(), base::VKEY_L, true, false, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsCurrentTabIndex(1));
  EXPECT_TRUE(clb_host()->IsVisible());
}

}  // namespace chromeos

