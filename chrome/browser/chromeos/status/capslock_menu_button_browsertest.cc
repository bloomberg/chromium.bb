// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/capslock_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/theme_resources.h"

namespace chromeos {

class CapslockMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  CapslockMenuButtonTest()
      : CrosInProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
  }

  CapslockMenuButton* GetCapslockMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->capslock_view();
  }
};

IN_PROC_BROWSER_TEST_F(CapslockMenuButtonTest, InitialIndicatorTest) {
  CapslockMenuButton* capslock = GetCapslockMenuButton();
  ASSERT_TRUE(capslock != NULL);

  // By default, the indicator should be disabled.
  EXPECT_FALSE(capslock->IsEnabled());
}

}  // namespace chromeos
