// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/accessibility_menu_button.h"

#include "chrome/browser/chromeos/accessibility_util.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"

namespace chromeos {

class AccessibilityMenuButtonTest : public InProcessBrowserTest {
 protected:
  AccessibilityMenuButtonTest() : InProcessBrowserTest() {
  }

  AccessibilityMenuButton* GetAccessibilityMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->accessibility_view();
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityMenuButtonTest,
                       VisibilityIsSyncedWithPreference) {
  AccessibilityMenuButton* button = GetAccessibilityMenuButton();
  ASSERT_TRUE(button != NULL);

  accessibility::EnableAccessibility(true, NULL);
  EXPECT_TRUE(button->IsVisible());

  accessibility::EnableAccessibility(false, NULL);
  EXPECT_FALSE(button->IsVisible());
}

}  // namespace chromeos
