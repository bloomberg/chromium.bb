// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/theme_installed_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;
using extensions::Extension;

class ExtensionInstallUIBrowserTest : public ExtensionBrowserTest {
 public:
  // Checks that a theme info bar is currently visible and issues an undo to
  // revert to the previous theme.
  void VerifyThemeInfoBarAndUndoInstall() {
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    ASSERT_EQ(1U, infobar_service->GetInfoBarCount());
    ConfirmInfoBarDelegate* delegate = infobar_service->
        GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
    ASSERT_TRUE(delegate);
    delegate->Cancel();
    ASSERT_EQ(0U, infobar_service->GetInfoBarCount());
  }

  // Install the given theme from the data dir and verify expected name.
  void InstallThemeAndVerify(const char* theme_name,
                             const std::string& expected_name) {
    const FilePath theme_path = test_data_dir_.AppendASCII(theme_name);
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_path, 1, browser()));
    const Extension* theme = GetTheme();
    ASSERT_TRUE(theme);
    ASSERT_EQ(theme->name(), expected_name);
  }

  const Extension* GetTheme() const {
    return ThemeServiceFactory::GetThemeForProfile(browser()->profile());
  }
};

#if defined(OS_LINUX)
// Fails consistently on bot chromium.chromiumos \ Linux.
// See: http://crbug.com/120647.
#define MAYBE_TestThemeInstallUndoResetsToDefault DISABLED_TestThemeInstallUndoResetsToDefault
#else
#define MAYBE_TestThemeInstallUndoResetsToDefault TestThemeInstallUndoResetsToDefault
#endif

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       MAYBE_TestThemeInstallUndoResetsToDefault) {
  // Install theme once and undo to verify we go back to default theme.
  FilePath theme_crx = PackExtension(test_data_dir_.AppendASCII("theme"));
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 1, browser()));
  const Extension* theme = GetTheme();
  ASSERT_TRUE(theme);
  std::string theme_id = theme->id();
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());

  // Set the same theme twice and undo to verify we go back to default theme.
  // We set the |expected_change| to zero in these 'InstallExtensionWithUI'
  // calls since the theme has already been installed above and this is an
  // overinstall to set the active theme.
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 0, browser()));
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(theme_crx, 0, browser()));
  theme = GetTheme();
  ASSERT_TRUE(theme);
  ASSERT_EQ(theme_id, theme->id());
  VerifyThemeInfoBarAndUndoInstall();
  ASSERT_EQ(NULL, GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestThemeInstallUndoResetsToPreviousTheme) {
  // Install first theme.
  InstallThemeAndVerify("theme", "camo theme");
  const Extension* theme = GetTheme();
  std::string theme_id = theme->id();

  // Then install second theme.
  InstallThemeAndVerify("theme2", "snowflake theme");
  const Extension* theme2 = GetTheme();
  EXPECT_FALSE(theme_id == theme2->id());

  // Undo second theme will revert to first theme.
  VerifyThemeInfoBarAndUndoInstall();
  EXPECT_EQ(theme, GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestThemeReset) {
  InstallThemeAndVerify("theme", "camo theme");

  // Reset to default theme.
  ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
  ASSERT_FALSE(GetTheme());
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       TestInstallThemeInFullScreen) {
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_FULLSCREEN));
  InstallThemeAndVerify("theme", "camo theme");
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       AppInstallConfirmation) {
  int num_tabs = browser()->tab_strip_model()->count();

  FilePath app_dir = test_data_dir_.AppendASCII("app");
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(app_dir, 1, browser()));

  if (NewTabUI::ShouldShowApps()) {
    EXPECT_EQ(num_tabs + 1, browser()->tab_strip_model()->count());
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(StartsWithASCII(web_contents->GetURL().spec(),
                                "chrome://newtab/", false));
  } else {
    // TODO(xiyuan): Figure out how to test extension installed bubble?
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallUIBrowserTest,
                       AppInstallConfirmation_Incognito) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  int num_incognito_tabs = incognito_browser->tab_strip_model()->count();
  int num_normal_tabs = browser()->tab_strip_model()->count();

  FilePath app_dir = test_data_dir_.AppendASCII("app");
  ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(app_dir, 1,
                                                incognito_browser));

  EXPECT_EQ(num_incognito_tabs,
            incognito_browser->tab_strip_model()->count());
  if (NewTabUI::ShouldShowApps()) {
    EXPECT_EQ(num_normal_tabs + 1, browser()->tab_strip_model()->count());
    WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    EXPECT_TRUE(StartsWithASCII(web_contents->GetURL().spec(),
                                "chrome://newtab/", false));
  } else {
    // TODO(xiyuan): Figure out how to test extension installed bubble?
  }
}
