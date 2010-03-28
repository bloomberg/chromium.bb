// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

class CompactNavigationBarTest : public InProcessBrowserTest {
 public:
  CompactNavigationBarTest() {
  }

  BrowserView* browser_view() const {
    return static_cast<BrowserView*>(browser()->window());
  }

  bool IsViewIdVisible(int id) const {
    return browser_view()->GetViewByID(id)->IsVisible();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactNavigationBarTest);
};

IN_PROC_BROWSER_TEST_F(CompactNavigationBarTest, TestBasic) {
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_TOOLBAR));

  browser()->ToggleCompactNavigationBar();
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_TOOLBAR));

  browser()->ToggleCompactNavigationBar();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_TOOLBAR));
}

IN_PROC_BROWSER_TEST_F(CompactNavigationBarTest, TestAccelerator) {
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));

  gfx::NativeWindow window = browser()->window()->GetNativeHandle();

  // ctrl-shift-c should toggle compact navigation bar.
  ui_controls::SendKeyPress(window, base::VKEY_C, true, true, false);
  RunAllPendingEvents();
  EXPECT_TRUE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));

  ui_controls::SendKeyPress(window, base::VKEY_C, true, true, false);
  RunAllPendingEvents();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));

  // but ctrl-alt-c should not.
  ui_controls::SendKeyPress(window, base::VKEY_C, true, false, true);
  RunAllPendingEvents();
  EXPECT_FALSE(IsViewIdVisible(VIEW_ID_COMPACT_NAV_BAR));
}

}  // namespace chromeos
