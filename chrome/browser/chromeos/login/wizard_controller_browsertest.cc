// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/login/wizard_in_process_browser_test.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "unicode/locid.h"

class WizardControllerTest : public chromeos::WizardInProcessBrowserTest {
 protected:
  WizardControllerTest(): chromeos::WizardInProcessBrowserTest("oobe") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  WizardController* const wizard = controller();
  ASSERT_TRUE(wizard != NULL);
  views::View* current_screen = wizard->contents();
  ASSERT_TRUE(current_screen != NULL);

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(current_screen->UILayoutIsRightToLeft());
  const std::wstring en_str = l10n_util::GetString(IDS_NETWORK_SELECTION_TITLE);

  wizard->OnSwitchLanguage("fr");
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(current_screen->UILayoutIsRightToLeft());
  const std::wstring fr_str = l10n_util::GetString(IDS_NETWORK_SELECTION_TITLE);

  EXPECT_NE(en_str, fr_str);

  wizard->OnSwitchLanguage("ar");
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(current_screen->UILayoutIsRightToLeft());
  const std::wstring ar_str = l10n_util::GetString(IDS_NETWORK_SELECTION_TITLE);

  EXPECT_NE(fr_str, ar_str);
}
