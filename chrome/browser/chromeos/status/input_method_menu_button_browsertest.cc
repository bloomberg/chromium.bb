// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/theme_resources.h"

#if defined(USE_AURA)
#include "chrome/browser/ui/views/aura/chrome_shell_delegate.h"
#endif

namespace chromeos {

class InputMethodMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  InputMethodMenuButtonTest()
      : CrosInProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    cros_mock_->InitStatusAreaMocks();
    cros_mock_->SetStatusAreaMocksExpectations();
  }

  const InputMethodMenuButton* GetInputMethodMenuButton() {
    const views::View* view =
#if defined(USE_AURA)
        ChromeShellDelegate::instance()->GetStatusAreaForTest();
#else
        static_cast<BrowserView*>(browser()->window());
#endif
    return static_cast<const InputMethodMenuButton*>(
        view->GetViewByID(VIEW_ID_STATUS_BUTTON_INPUT_METHOD));
  }
};

IN_PROC_BROWSER_TEST_F(InputMethodMenuButtonTest, InitialIndicatorTest) {
  const InputMethodMenuButton* input_method = GetInputMethodMenuButton();
  ASSERT_TRUE(input_method != NULL);

  // By default, show the indicator of the hardware keyboard, which is set
  // to US for tests.
  EXPECT_EQ(ASCIIToUTF16("US"), input_method->text());
}

}  // namespace chromeos
