// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/language_menu_button.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/cros/cros_in_process_browser_test.h"
#include "chrome/browser/chromeos/cros/mock_language_library.h"
#include "chrome/browser/chromeos/frame/browser_view.h"
#include "chrome/browser/chromeos/status/browser_status_area_view.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "grit/theme_resources.h"

namespace chromeos {

using ::testing::Return;
using ::testing::ReturnRef;

class LanguageMenuButtonTest : public CrosInProcessBrowserTest {
 protected:
  LanguageMenuButtonTest()
      : CrosInProcessBrowserTest(),
        korean_desc_("hangul",  // The engine id for ibus-hangul.
                     "Korean",  // The display name for the input method.
                     "us",  // The input method assumes US keyboard layout.
                     "ko"),  // The input method is for Korean language.
        persian_desc_("m17n:fa:isiri",  // The engine id for ibus-m17n
                      "isiri (m17n)",  // The display name for the input method.
                      "us",  // The input method assumes US keyboard layout.
                      "fa"),  // The input method is for Farsi/Persian language.
        invalid_desc_("invalid-id",
                      "unregistered string",
                      "us",
                      "xx")  // Unknown language code.
  {}

  virtual void SetUpInProcessBrowserTestFixture() {
    InitStatusAreaMocks();
    SetStatusAreaMocksExpectations();
  }

  LanguageMenuButton* GetLanguageMenuButton() {
    BrowserView* view = static_cast<BrowserView*>(browser()->window());
    return static_cast<BrowserStatusAreaView*>(view->
        GetViewByID(VIEW_ID_STATUS_AREA))->language_view();
  }

  InputMethodDescriptor korean_desc_;
  InputMethodDescriptor persian_desc_;
  InputMethodDescriptor invalid_desc_;
};

IN_PROC_BROWSER_TEST_F(LanguageMenuButtonTest, InitialIndicatorTest) {
  LanguageMenuButton* language = GetLanguageMenuButton();
  ASSERT_TRUE(language != NULL);

  // Since the default input method is "xkb:us::eng", "EN" should be set for the
  // indicator.
  std::wstring indicator = language->text();
  EXPECT_EQ(L"EN", indicator);
}

IN_PROC_BROWSER_TEST_F(LanguageMenuButtonTest, IndicatorAndTooltipUpdateTest) {
  EXPECT_CALL(*mock_language_library_, previous_input_method())
      .Times(3)
      .WillOnce(ReturnRef(invalid_desc_))
      .WillOnce(ReturnRef(invalid_desc_))
      .WillOnce(ReturnRef(invalid_desc_))
      .RetiresOnSaturation();
  EXPECT_CALL(*mock_language_library_, current_input_method())
      .Times(3)
      .WillOnce(ReturnRef(korean_desc_))
      .WillOnce(ReturnRef(persian_desc_))
      .WillOnce(ReturnRef(invalid_desc_))
      .RetiresOnSaturation();
  LanguageMenuButton* language = GetLanguageMenuButton();
  ASSERT_TRUE(language != NULL);

  language->InputMethodChanged(mock_language_library_);
  std::wstring indicator = language->text();
  EXPECT_EQ(L"KO", indicator);
  std::wstring tooltip;
  language->GetTooltipText(gfx::Point(), &tooltip);
  EXPECT_EQ(L"Korean - Korean input method", tooltip);

  language->InputMethodChanged(mock_language_library_);
  indicator = language->text();
  EXPECT_EQ(L"FA", indicator);
  language->GetTooltipText(gfx::Point(), &tooltip);
  EXPECT_EQ(L"Persian - Persian input method (ISIRI 2901 layout)", tooltip);

  language->InputMethodChanged(mock_language_library_);
  indicator = language->text();
  EXPECT_EQ(L"XX", indicator);
  language->GetTooltipText(gfx::Point(), &tooltip);
  EXPECT_EQ(L"xx - unregistered string", tooltip);
}

}  // namespace chromeos
