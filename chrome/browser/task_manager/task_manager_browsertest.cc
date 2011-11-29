// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background/background_contents_service.h"
#include "chrome/browser/background/background_contents_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/page_transition_types.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// On Linux this is crashing intermittently http://crbug/84719
// In some environments this test fails about 1/6 http://crbug/84850
#if defined(OS_LINUX)
#define MAYBE_KillExtension DISABLED_KillExtension
#else
#define MAYBE_KillExtension KillExtension
#endif

namespace {

const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");

}  // namespace

class TaskManagerBrowserTest : public ExtensionBrowserTest {
 public:
  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }
};

// Flaky crashes on ChromeOS (triggers pure virtual function call), see
// http://crbug.com/92297 for details
#if defined(OS_CHROMEOS) || defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ShutdownWhileOpen DISABLED_ShutdownWhileOpen
#else
#define MAYBE_ShutdownWhileOpen ShutdownWhileOpen
#endif

// Regression test for http://crbug.com/13361
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_ShutdownWhileOpen) {
  browser()->window()->ShowTaskManager();
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  // Check that the third entry is a tab contents resource whose title starts
  // starts with "Tab:".
  ASSERT_TRUE(model()->GetResourceTabContents(2) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

  // Close the tab and verify that we notice.
  TabContents* first_tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(first_tab);
  browser()->CloseTabContents(first_tab);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeBGContentsChanges) {
  EXPECT_EQ(0, model()->ResourceCount());
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Open a new background contents and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));

  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile());
  string16 application_id(ASCIIToUTF16("test_app_id"));
  service->LoadBackgroundContents(browser()->profile(),
                                  url,
                                  ASCIIToUTF16("background_page"),
                                  application_id);
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  EXPECT_EQ(1, TaskManager::GetBackgroundPageCount());

  // Close the background contents and verify that we notice.
  service->ShutdownAssociatedBackgroundContents(application_id);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillBGContents) {
  EXPECT_EQ(0, model()->ResourceCount());
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Open a new background contents and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));

  ui_test_utils::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BACKGROUND_CONTENTS_NAVIGATED,
      content::Source<Profile>(browser()->profile()));

  BackgroundContentsService* service =
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile());
  string16 application_id(ASCIIToUTF16("test_app_id"));
  service->LoadBackgroundContents(browser()->profile(),
                                  url,
                                  ASCIIToUTF16("background_page"),
                                  application_id);

  // Wait for the background contents process to finish loading.
  observer.Wait();

  EXPECT_EQ(3, model()->ResourceCount());
  EXPECT_EQ(1, TaskManager::GetBackgroundPageCount());

  // Kill the background contents process and verify that it disappears from the
  // model.
  bool found = false;
  for (int i = 0; i < model()->ResourceCount(); ++i) {
    if (model()->IsBackgroundResource(i)) {
      TaskManager::GetInstance()->KillProcess(i);
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionChanges) {
  EXPECT_EQ(0, model()->ResourceCount());
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Loading an extension with a background page should result in a new
  // resource being created for it.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  EXPECT_EQ(1, TaskManager::GetBackgroundPageCount());

  // Unload extension to avoid crash on Windows (see http://crbug.com/31663).
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTabs) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();
  // Wait for loading of task manager.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, Extension background page, and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  // Open a new tab to an extension URL and make sure we notice that.
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForResourceChange(4);

  // Check that the third entry (background) is an extension resource whose
  // title starts with "Extension:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(2));
  ASSERT_TRUE(model()->GetResourceTabContents(2) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_EXTENSION_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

  // Check that the fourth entry (page.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "Extension:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(3));
  ASSERT_TRUE(model()->GetResourceTabContents(3) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(3) != NULL);
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(3), prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabs) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();
  // Wait for loading of task manager.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "App:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(2));
  ASSERT_TRUE(model()->GetResourceTabContents(2) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) == extension);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_APP_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabs) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  GURL base_url = test_server()->GetURL(
      "files/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Check that the third entry's title starts with "Tab:".
  string16 tab_prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), tab_prefix, true));

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), tab_prefix, true));

  // Now reload and check that the third entry's title now starts with "App:".
  ui_test_utils::NavigateToURL(browser(), url);
  string16 app_prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_APP_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), app_prefix, true));

  // Disable extension and reload page.
  DisableExtension(last_loaded_extension_id_);
  ui_test_utils::NavigateToURL(browser(), url);

  // The third entry's title should be back to a normal tab.
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), tab_prefix, true));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_KillExtension) {
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();
  // Wait for loading of task manager.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  EXPECT_EQ(1, TaskManager::GetBackgroundPageCount());

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  // Kill the extension process and make sure we notice it.
  TaskManager::GetInstance()->KillProcess(2);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);
  EXPECT_EQ(0, TaskManager::GetBackgroundPageCount());
}

// Disabled, http://crbug.com/66957.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_KillExtensionAndReload) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();
  // Wait for loading of task manager.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  // Kill the extension process and make sure we notice it.
  TaskManager::GetInstance()->KillProcess(2);
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Reload the extension using the "crashed extension" infobar while the task
  // manager is still visible. Make sure we don't crash and the extension
  // gets reloaded and noticed in the task manager.
  InfoBarTabHelper* infobar_helper =
      browser()->GetSelectedTabContentsWrapper()->infobar_tab_helper();
  ASSERT_EQ(1U, infobar_helper->infobar_count());
  ConfirmInfoBarDelegate* delegate = infobar_helper->
      GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  delegate->Accept();
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
}

#if defined(OS_LINUX) || defined(OS_WIN)
// http://crbug.com/93158.
#define MAYBE_ReloadExtension FLAKY_ReloadExtension
#else
#define MAYBE_ReloadExtension ReloadExtension
#endif

// Regression test for http://crbug.com/18693.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_ReloadExtension) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();
  // Wait for loading of task manager.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  LOG(INFO) << "loading extension";
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  LOG(INFO) << "waiting for resource change";
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  const Extension* extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  // Reload the extension a few times and make sure our resource count
  // doesn't increase.
  LOG(INFO) << "First extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Second extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Third extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);
}

// Crashy, http://crbug.com/42301.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_PopulateWebCacheFields) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForResourceChange(3);

  // Check that we get some value for the cache columns.
  DCHECK_NE(model()->GetResourceWebCoreImageCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreScriptsCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreCSSCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
}
