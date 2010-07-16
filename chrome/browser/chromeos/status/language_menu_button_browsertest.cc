// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_input_method_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/theme_resources.h"

namespace chromeos {

class LanguageMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  LanguageMenuButtonTest()
      : CrosInProcessBrowserTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();
  }

  LanguageMenuButton* GetLanguageMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<StatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->language_view();
  }
};

IN_PROC_BROWSER_TEST_F(LanguageMenuButtonTest, InitialIndicatorTest) {
  LanguageMenuButton* language = GetLanguageMenuButton();
  ASSERT_TRUE(language != NULL);

  // Since the default input method is "xkb:us::eng", "US" should be set for the
  // indicator.
  std::wstring indicator = language->text();
  EXPECT_EQ(L"US", indicator);
}

}  // namespace chromeos
