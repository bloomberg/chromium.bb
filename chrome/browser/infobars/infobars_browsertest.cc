// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/browser/notification_service.h"
#include "net/test/test_server.h"

class InfoBarsTest : public InProcessBrowserTest {
 public:
  InfoBarsTest() {}

  void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(
        switches::kAppsGalleryInstallAutoConfirmForTests, "accept");
  }

  void InstallExtension(const char* filename) {
    FilePath path = ui_test_utils::GetTestFilePath(
        FilePath().AppendASCII("extensions"), FilePath().AppendASCII(filename));
    Profile* profile = browser()->profile();
    ExtensionService* service = profile->GetExtensionService();

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::NotificationService::AllSources());

    ExtensionInstallPrompt* client = new ExtensionInstallPrompt(
        browser()->tab_strip_model()->GetActiveWebContents());
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, client));
    installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
    installer->InstallCrx(path);

    observer.Wait();
  }
};

IN_PROC_BROWSER_TEST_F(InfoBarsTest, TestInfoBarsCloseOnNewTheme) {
  ASSERT_TRUE(test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("files/simple.html"));

  content::WindowedNotificationObserver infobar_added_1(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  InstallExtension("theme.crx");
  infobar_added_1.Wait();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_server()->GetURL("files/simple.html"),
      NEW_FOREGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WindowedNotificationObserver infobar_added_2(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());
  content::WindowedNotificationObserver infobar_removed_1(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
  InstallExtension("theme2.crx");
  infobar_added_2.Wait();
  infobar_removed_1.Wait();
  EXPECT_EQ(0u,
            InfoBarService::FromWebContents(browser()->tab_strip_model()->
                GetWebContentsAt(0))->GetInfoBarCount());

  content::WindowedNotificationObserver infobar_removed_2(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
        content::NotificationService::AllSources());
  ThemeServiceFactory::GetForProfile(browser()->profile())->UseDefaultTheme();
  infobar_removed_2.Wait();
  EXPECT_EQ(0u,
            InfoBarService::FromWebContents(browser()->tab_strip_model()->
                GetActiveWebContents())->GetInfoBarCount());
}
