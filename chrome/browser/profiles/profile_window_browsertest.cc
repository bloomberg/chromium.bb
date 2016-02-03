// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include <stddef.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// This test verifies the Desktop implementation of Guest only.
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)

namespace {

// Code related to history borrowed from:
// chrome/browser/history/history_browsertest.cc

// Note: WaitableEvent is not used for synchronization between the main thread
// and history backend thread because the history subsystem posts tasks back
// to the main thread. Had we tried to Signal an event in such a task
// and Wait for it on the main thread, the task would not run at all because
// the main thread would be blocked on the Wait call, resulting in a deadlock.

// A task to be scheduled on the history backend thread.
// Notifies the main thread after all history backend thread tasks have run.
class WaitForHistoryTask : public history::HistoryDBTask {
 public:
  WaitForHistoryTask() {}

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }

  void DoneRunOnMainThread() override {
    base::MessageLoop::current()->QuitWhenIdle();
  }

 private:
  ~WaitForHistoryTask() override {}

  DISALLOW_COPY_AND_ASSIGN(WaitForHistoryTask);
};

void WaitForHistoryBackendToRun(Profile* profile) {
  base::CancelableTaskTracker task_tracker;
  scoped_ptr<history::HistoryDBTask> task(new WaitForHistoryTask());
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->ScheduleDBTask(std::move(task), &task_tracker);
  content::RunMessageLoop();
}

class EmptyAcceleratorHandler : public ui::AcceleratorProvider {
 public:
  // Don't handle accelerators.
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) override {
    return false;
  }
};

}  // namespace

class ProfileWindowBrowserTest : public InProcessBrowserTest {
 public:
  ProfileWindowBrowserTest() {}
  ~ProfileWindowBrowserTest() override {}

  Browser* OpenGuestBrowser();
  void CloseBrowser(Browser* browser);

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileWindowBrowserTest);
};

Browser* ProfileWindowBrowserTest::OpenGuestBrowser() {
  size_t num_browsers = BrowserList::GetInstance()->size();

  // Create a guest browser nicely. Using CreateProfile() and CreateBrowser()
  // does incomplete initialization that would lead to
  // SystemUrlRequestContextGetter being leaked.
  content::WindowedNotificationObserver browser_creation_observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  profiles::SwitchToGuestProfile(chrome::GetActiveDesktop(),
                                 ProfileManager::CreateCallback());

  browser_creation_observer.Wait();
  DCHECK_NE(static_cast<Profile*>(nullptr),
            g_browser_process->profile_manager()->GetProfileByPath(
                ProfileManager::GetGuestProfilePath()));
  EXPECT_EQ(num_browsers + 1, BrowserList::GetInstance()->size());

  Profile* guest = g_browser_process->profile_manager()->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());
  Browser* browser = chrome::FindAnyBrowser(guest, true);
  EXPECT_TRUE(browser);

  // When |browser| closes a BrowsingDataRemover will be created and executed.
  // It needs a loaded TemplateUrlService or else it hangs on to a
  // CallbackList::Subscription forever.
  search_test_utils::WaitForTemplateURLServiceToLoad(
      TemplateURLServiceFactory::GetForProfile(guest));

  return browser;
}

void ProfileWindowBrowserTest::CloseBrowser(Browser* browser) {
  content::WindowedNotificationObserver window_close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(browser));
  browser->window()->Close();
  window_close_observer.Wait();
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, OpenGuestBrowser) {
  EXPECT_TRUE(OpenGuestBrowser());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIsIncognito) {
  Browser* guest_browser = OpenGuestBrowser();
  EXPECT_TRUE(guest_browser->profile()->IsOffTheRecord());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestIgnoresHistory) {
  Browser* guest_browser = OpenGuestBrowser();

  ui_test_utils::WaitForHistoryToLoad(HistoryServiceFactory::GetForProfile(
      guest_browser->profile(), ServiceAccessType::EXPLICIT_ACCESS));

  GURL test_url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title2.html")));

  ui_test_utils::NavigateToURL(guest_browser, test_url);
  WaitForHistoryBackendToRun(guest_browser->profile());

  std::vector<GURL> urls =
      ui_test_utils::HistoryEnumerator(guest_browser->profile()).urls();
  ASSERT_EQ(0U, urls.size());
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestClearsCookies) {
  Browser* guest_browser = OpenGuestBrowser();
  Profile* guest_profile = guest_browser->profile();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/set-cookie?cookie1"));

  // Before navigation there are no cookies for the URL.
  std::string cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);

  // After navigation there is a cookie for the URL.
  ui_test_utils::NavigateToURL(guest_browser, url);
  cookie = content::GetCookies(guest_profile, url);
  EXPECT_EQ("cookie1", cookie);

  CloseBrowser(guest_browser);

  // Closing the browser has removed the cookie.
  cookie = content::GetCookies(guest_profile, url);
  ASSERT_EQ("", cookie);
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestCannotSignin) {
  Browser* guest_browser = OpenGuestBrowser();

  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(
      guest_browser->profile());

  // Guest profiles can't sign in without a SigninManager.
  ASSERT_FALSE(signin_manager);
}

IN_PROC_BROWSER_TEST_F(ProfileWindowBrowserTest, GuestAppMenuLacksBookmarks) {
  EmptyAcceleratorHandler accelerator_handler;
  // Verify the normal browser has a bookmark menu.
  AppMenuModel model_normal_profile(&accelerator_handler, browser());
  EXPECT_NE(-1, model_normal_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));

  // Guest browser has no bookmark menu.
  Browser* guest_browser = OpenGuestBrowser();
  AppMenuModel model_guest_profile(&accelerator_handler, guest_browser);
  EXPECT_EQ(-1, model_guest_profile.GetIndexOfCommandId(IDC_BOOKMARKS_MENU));
}

#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID) && !defined(OS_IOS)
