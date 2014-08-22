// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyExtension;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchExtension;
using task_manager::browsertest_util::MatchTab;
using task_manager::browsertest_util::WaitForTaskManagerRows;

namespace {

const base::FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");

}  // namespace

class TaskManagerBrowserTest : public ExtensionBrowserTest {
 public:
  TaskManagerBrowserTest() {}
  virtual ~TaskManagerBrowserTest() {}

  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }

  void ShowTaskManager() {
    EXPECT_EQ(0, model()->ResourceCount());

    // Show the task manager. This populates the model, and helps with debugging
    // (you see the task manager).
    chrome::ShowTaskManager(browser());
  }

  void Refresh() {
    model()->Refresh();
  }

  int GetUpdateTimeMs() {
    return TaskManagerModel::kUpdateTimeMs;
  }

  GURL GetTestURL() {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kTitle1File));
  }

  int FindResourceIndex(const base::string16& title) {
    for (int i = 0; i < model()->ResourceCount(); ++i) {
      if (title == model()->GetResourceTitle(i))
        return i;
    }
    return -1;
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not launch device discovery process.
    command_line->AppendSwitch(switches::kDisableDeviceDiscoveryNotifications);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserTest);
};

#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ShutdownWhileOpen DISABLED_ShutdownWhileOpen
#else
#define MAYBE_ShutdownWhileOpen ShutdownWhileOpen
#endif

// Regression test for http://crbug.com/13361
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_ShutdownWhileOpen) {
  ShowTaskManager();
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, GetTestURL(), content::PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Close the tab and verify that we notice.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillTab) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, GetTestURL(), content::PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Killing the tab via task manager should remove the row.
  int tab = FindResourceIndex(MatchTab("title1.html"));
  ASSERT_NE(-1, tab);
  ASSERT_TRUE(model()->GetResourceWebContents(tab) != NULL);
  ASSERT_TRUE(model()->CanActivate(tab));
  TaskManager::GetInstance()->KillProcess(tab);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Tab should reappear in task manager upon reload.
  chrome::Reload(browser(), CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticePanel) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Open a new panel to an extension url.
  GURL url(
    "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/french_sentence.html");
  Panel* panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);

  // Make sure that a task manager model created after the panel shows the
  // existence of the panel and the extension.
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Close the panel and verify that we notice.
  panel->Close();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      0,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticePanelChanges) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, the New Tab Page and Extension background page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Open a new panel to an extension url and make sure we notice that.
  GURL url(
    "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/french_sentence.html");
  Panel* panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Close the panel and verify that we notice.
  panel->Close();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      0,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));

  // Unload extension.
  UnloadExtension(last_loaded_extension_id());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

// Kills a process that has more than one task manager entry.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillPanelViaExtensionResource) {
  ShowTaskManager();
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                .AppendASCII("Extensions")
                                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                                .AppendASCII("1.0.0.0")));

  // Open a new panel to an extension url.
  GURL url(
      "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
      "french_sentence.html");
  PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Kill the process via the BACKGROUND PAGE (not the panel). Verify that both
  // the background page and the panel go away from the task manager.
  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_TRUE(model()->GetResourceWebContents(background_page) == NULL);
  ASSERT_FALSE(model()->CanActivate(background_page));
  TaskManager::GetInstance()->KillProcess(background_page);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

// Kills a process that has more than one task manager entry. This test is the
// same as KillPanelViaExtensionResource except it does the kill via the other
// entry.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillPanelViaPanelResource) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                .AppendASCII("Extensions")
                                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                                .AppendASCII("1.0.0.0")));

  // Open a new panel to an extension url.
  GURL url(
      "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
      "french_sentence.html");
  PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1,
      MatchExtension(
          "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
          "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_TRUE(model()->GetResourceWebContents(background_page) == NULL);
  ASSERT_FALSE(model()->CanActivate(background_page));

  // Kill the process via the PANEL RESOURCE (not the background page). Verify
  // that both the background page and the panel go away from the task manager.
  int panel = FindResourceIndex(MatchExtension(
      "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
      "french_sentence.html"));
  ASSERT_NE(-1, panel);
  ASSERT_TRUE(model()->GetResourceWebContents(panel) != NULL);
  ASSERT_TRUE(model()->CanActivate(panel));
  TaskManager::GetInstance()->KillProcess(panel);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTabChanges) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, Extension background page, and the New Tab Page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Open a new tab to an extension URL. Afterwards, the third entry (background
  // page) should be an extension resource whose title starts with "Extension:".
  // The fourth entry (page.html) is also of type extension and has both a
  // WebContents and an extension. The title should start with "Extension:".
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_TRUE(model()->GetResourceWebContents(extension_tab) != NULL);
  ASSERT_TRUE(model()->CanActivate(extension_tab));

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_TRUE(model()->GetResourceWebContents(background_page) == NULL);
  ASSERT_FALSE(model()->CanActivate(background_page));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTab) {
  // With the task manager closed, open a new tab to an extension URL.
  // Afterwards, when we open the task manager, the third entry (background
  // page) should be an extension resource whose title starts with "Extension:".
  // The fourth entry (page.html) is also of type extension and has both a
  // WebContents and an extension. The title should start with "Extension:".
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                .AppendASCII("Extensions")
                                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                                .AppendASCII("1.0.0.0")));
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_TRUE(model()->GetResourceWebContents(extension_tab) != NULL);
  ASSERT_TRUE(model()->CanActivate(extension_tab));

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_TRUE(model()->GetResourceWebContents(background_page) == NULL);
  ASSERT_FALSE(model()->CanActivate(background_page));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabChanges) {
  ShowTaskManager();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = extensions::ExtensionSystem::Get(
                                  browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);

  // There should be 1 "App: " tab and the original new tab page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  int app_tab = FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_NE(-1, app_tab);
  ASSERT_TRUE(model()->GetResourceWebContents(app_tab) != NULL);
  ASSERT_TRUE(model()->CanActivate(app_tab));
  ASSERT_EQ(task_manager::Resource::EXTENSION,
            model()->GetResourceType(app_tab));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Unload extension to make sure the tab goes away.
  UnloadExtension(last_loaded_extension_id());

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTab) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);

  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  int app_tab = FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_NE(-1, app_tab);
  ASSERT_TRUE(model()->GetResourceWebContents(app_tab) != NULL);
  ASSERT_TRUE(model()->CanActivate(app_tab));
  ASSERT_EQ(task_manager::Resource::EXTENSION,
            model()->GetResourceType(app_tab));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabChanges) {
  ShowTaskManager();

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Check that the new entry's title starts with "Tab:".
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));
  // Force the TaskManager to query the title.
  Refresh();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Now reload and check that the last entry's title now starts with "App:".
  ui_test_utils::NavigateToURL(browser(), url);

  // Force the TaskManager to query the title.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Disable extension.
  DisableExtension(last_loaded_extension_id());

  // The hosted app should now show up as a normal "Tab: ".
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));

  // Reload the page.
  ui_test_utils::NavigateToURL(browser(), url);

  // No change expected.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabAfterReload) {
  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  GURL base_url =
      embedded_test_server()->GetURL("/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));

  // Now reload, which should transition this tab to being an App.
  ui_test_utils::NavigateToURL(browser(), url);

  ShowTaskManager();

  // The TaskManager should show this as an "App: "
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabBeforeReload) {
  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // must stay in scope with replace_host
  replace_host.SetHostStr(host_str);
  GURL base_url =
      embedded_test_server()->GetURL("/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));

  ShowTaskManager();

  // The TaskManager should show this as a "Tab: " because the page hasn't been
  // reloaded since the hosted app was installed.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
}

// Regression test for http://crbug.com/18693.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ReloadExtension) {
  ShowTaskManager();
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("background_page")));

  // Reload the extension a few times and make sure our resource count doesn't
  // increase.
  std::string extension_id = last_loaded_extension_id();
  for (int i = 1; i <= 5; i++) {
    SCOPED_TRACE(testing::Message() << "Reloading extension for the " << i
                                    << "th time.");
    ReloadExtension(extension_id);
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchExtension("background_page")));
  }
}

// Crashy, http://crbug.com/42301.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_PopulateWebCacheFields) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();

  // Open a new tab and make sure we notice that.
  AddTabAtIndex(0, GetTestURL(), content::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Check that we get some value for the cache columns.
  DCHECK_NE(model()->GetResourceWebCoreImageCacheSize(resource_count),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreScriptsCacheSize(resource_count),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreCSSCacheSize(resource_count),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
}

// Checks that task manager counts a worker thread JS heap size.
// http://crbug.com/241066
// Flaky, http://crbug.com/259368
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DISABLED_WebWorkerJSHeapMemory) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  const int extra_timeout_ms = 500;
  size_t minimal_heap_size = 2 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "var blob = new Blob([\n"
      "    'mem = new Array(%lu);',\n"
      "    'for (var i = 0; i < mem.length; i += 16) mem[i] = i;',\n"
      "    'postMessage();']);\n"
      "blobURL = window.URL.createObjectURL(blob);\n"
      "worker = new Worker(blobURL);\n"
      "// Give the task manager few seconds to poll for JS heap sizes.\n"
      "worker.onmessage = setTimeout.bind(\n"
      "    this,\n"
      "    function () { window.domAutomationController.send(true); },\n"
      "    %d);\n"
      "worker.postMessage();\n",
      static_cast<unsigned long>(minimal_heap_size),
      GetUpdateTimeMs() + extra_timeout_ms);
  bool ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_TRUE(ok);

  int resource_index = TaskManager::GetInstance()->model()->ResourceCount() - 1;
  size_t result;

  ASSERT_TRUE(model()->GetV8Memory(resource_index, &result));
  LOG(INFO) << "Got V8 Heap Size " << result << " bytes";
  EXPECT_GE(result, minimal_heap_size);

  ASSERT_TRUE(model()->GetV8MemoryUsed(resource_index, &result));
  LOG(INFO) << "Got V8 Used Heap Size " << result << " bytes";
  EXPECT_GE(result, minimal_heap_size);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewDockedWindow) {
  ShowTaskManager();  // Task manager shown BEFORE dev tools window.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewUndockedWindow) {
  ShowTaskManager();  // Task manager shown BEFORE dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldDockedWindow) {
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldUnockedWindow) {
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}
