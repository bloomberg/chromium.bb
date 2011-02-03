// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/input_method_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/theme_resources.h"

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

  InputMethodMenuButton* GetInputMethodMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->input_method_view();
  }
};

IN_PROC_BROWSER_TEST_F(InputMethodMenuButtonTest, InitialIndicatorTest) {
  InputMethodMenuButton* input_method = GetInputMethodMenuButton();
  ASSERT_TRUE(input_method != NULL);

  // By default, show the indicator of the hardware keyboard, which is set
  // to US for tests.
  std::wstring indicator = input_method->text();
  EXPECT_EQ(L"US", indicator);
}

}  // namespace chromeos
