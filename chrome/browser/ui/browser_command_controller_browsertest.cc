// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_command_controller.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tab_modal_confirm_dialog_browsertest.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

class BrowserCommandControllerBrowserTest: public InProcessBrowserTest {
 public:
  BrowserCommandControllerBrowserTest() {}
  ~BrowserCommandControllerBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCommandControllerBrowserTest);
};

// Verify that showing a constrained window disables find.
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest, DisableFind) {
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Showing constrained window should disable find.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  MockTabModalConfirmDialogDelegate* delegate =
      new MockTabModalConfirmDialogDelegate(web_contents, NULL);
  TabModalConfirmDialog::Create(delegate, web_contents);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching to a new (unblocked) tab should reenable it.
  AddBlankTabAndShow(browser());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Switching back to the blocked tab should disable it again.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_FIND));

  // Closing the constrained window should reenable it.
  delegate->Cancel();
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_FIND));
}

// Note that a Browser's destructor, when the browser's profile is guest, will
// create and execute a BrowsingDataRemover.
// Flakes http://crbug.com/471953
IN_PROC_BROWSER_TEST_F(BrowserCommandControllerBrowserTest,
                       DISABLED_NewAvatarMenuEnabledInGuestMode) {
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Create a guest browser nicely. Using CreateProfile() and CreateBrowser()
  // does incomplete initialization that would lead to
  // SystemUrlRequestContextGetter being leaked.
  content::WindowedNotificationObserver browser_creation_observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  profiles::SwitchToGuestProfile(ProfileManager::CreateCallback());

  // RunUntilIdle() (racily) isn't sufficient to ensure browser creation, so
  // listen for the notification.
  base::MessageLoop::current()->RunUntilIdle();
  browser_creation_observer.Wait();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  // Access the browser that was created for the new Guest Profile.
  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* browser = chrome::FindAnyBrowser(guest, true);
  EXPECT_TRUE(browser);

  // The BrowsingDataRemover needs a loaded TemplateUrlService or else it hangs
  // on to a CallbackList::Subscription forever.
  TemplateURLServiceFactory::GetForProfile(guest)->set_loaded(true);

  const CommandUpdater* command_updater =
      browser->command_controller()->command_updater();
  #if defined(OS_CHROMEOS)
    // Chrome OS uses system tray menu to handle multi-profiles.
    EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
  #else
    EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_AVATAR_MENU));
  #endif
}
