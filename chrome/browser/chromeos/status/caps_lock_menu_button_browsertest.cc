// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/caps_lock_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/theme_resources.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#endif

namespace chromeos {

class CapsLockMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  CapsLockMenuButtonTest()
      : CrosInProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
  }

  const CapsLockMenuButton* GetCapsLockMenuButton() {
    const views::View* view =
#if defined(USE_AURA)
        ChromeShellDelegate::instance()->GetStatusArea();
#else
        static_cast<BrowserView*>(browser()->window());
#endif
    return static_cast<const CapsLockMenuButton*>(
        view->GetViewByID(VIEW_ID_STATUS_BUTTON_CAPS_LOCK));
  }
};

IN_PROC_BROWSER_TEST_F(CapsLockMenuButtonTest, InitialIndicatorTest) {
  const CapsLockMenuButton* caps_lock = GetCapsLockMenuButton();
  ASSERT_TRUE(caps_lock != NULL);

  // By default, the indicator shouldn't be shown.
  EXPECT_FALSE(caps_lock->IsVisible());
}

}  // namespace chromeos
