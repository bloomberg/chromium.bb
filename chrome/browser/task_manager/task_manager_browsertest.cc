// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
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
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

using content::WebContents;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyExtension;
using task_manager::browsertest_util::MatchAnySubframe;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyUtility;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchExtension;
using task_manager::browsertest_util::MatchSubframe;
using task_manager::browsertest_util::MatchTab;
using task_manager::browsertest_util::MatchUtility;
using task_manager::browsertest_util::TaskManagerTester;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using task_manager::browsertest_util::WaitForTaskManagerStatToExceed;

namespace {

const base::FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");

}  // namespace

// TODO(nick): Move this file into task_management. https://crbug.com/606963
class TaskManagerBrowserTest : public ExtensionBrowserTest {
 public:
  TaskManagerBrowserTest() {}
  ~TaskManagerBrowserTest() override {}

  TaskManagerTester* model() { return model_.get(); }

  void ShowTaskManager() {
    // Show the task manager. This populates the model, and helps with debugging
    // (you see the task manager).
    chrome::ShowTaskManager(browser());
    model_ = task_manager::browsertest_util::GetTaskManagerTester();
  }

  void HideTaskManager() {
    model_.reset();

    // Hide the task manager, and wait for it to go.
    chrome::HideTaskManager();
    base::RunLoop().RunUntilIdle();  // OnWindowClosed happens asynchronously.
  }

  void Refresh() {
    // Refresh() isn't ever needed on the new task manager.
    if (switches::NewTaskManagerEnabled())
      return;
    TaskManager::GetInstance()->model()->Refresh();
  }

  GURL GetTestURL() {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kTitle1File));
  }

  int FindResourceIndex(const base::string16& title) {
    for (int i = 0; i < model_->GetRowCount(); ++i) {
      if (title == model_->GetRowTitle(i))
        return i;
    }
    return -1;
  }

  // TODO(nick, afakhry): Remove this function. https://crbug.com/606963
  void DisableNewTaskManagerForBrokenTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableNewTaskManager);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not launch device discovery process.
    command_line->AppendSwitch(switches::kDisableDeviceDiscoveryNotifications);
  }

  void TearDownOnMainThread() override { model_.reset(); }

 private:
  std::unique_ptr<TaskManagerTester> model_;
  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserTest);
};

class TaskManagerUtilityProcessBrowserTest : public TaskManagerBrowserTest {
 public:
  TaskManagerUtilityProcessBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);

    // Enable out-of-process proxy resolver. Use a trivial PAC script to ensure
    // that some javascript is being executed.
    command_line->AppendSwitch(switches::kV8PacMojoOutOfProcess);
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        "data:,function FindProxyForURL(url, host){return \"DIRECT;\";}");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerUtilityProcessBrowserTest);
};

// Parameterized variant of TaskManagerBrowserTest which runs with/without
// --site-per-process, which enables out of process iframes (OOPIFs).
class TaskManagerOOPIFBrowserTest : public TaskManagerBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  TaskManagerOOPIFBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);
    if (GetParam())
      content::IsolateAllSitesForTesting(command_line);
  }

  bool ShouldExpectSubframes() {
    return content::AreAllSitesIsolatedForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerOOPIFBrowserTest);
};

INSTANTIATE_TEST_CASE_P(, TaskManagerOOPIFBrowserTest, ::testing::Bool());

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
  AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED);

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
  AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Killing the tab via task manager should remove the row.
  int tab = FindResourceIndex(MatchTab("title1.html"));
  ASSERT_NE(-1, tab);
  ASSERT_NE(-1, model()->GetTabId(tab));
  model()->Kill(tab);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Tab should reappear in task manager upon reload.
  chrome::Reload(browser(), CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
}

// Regression test for http://crbug.com/444945.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NavigateAwayFromHungRenderer) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  GURL url1(embedded_test_server()->GetURL("/title2.html"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/iframe.html"));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, url1, ui::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // Initiate a navigation that will create a new WebContents in the same
  // SiteInstance. Then immediately hang the renderer so that title3.html can't
  // load in this process.
  content::WebContentsAddedObserver web_contents_added_observer;
  int dummy_value = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      tab1->GetMainFrame(),
      "window.open('title3.html', '_blank');\n"
      "window.domAutomationController.send(55);\n"
      "while(1);",
      &dummy_value));

  // Blocks until a new WebContents appears as a result of window.open().
  WebContents* tab2 = web_contents_added_observer.GetWebContents();

  // Make sure the new WebContents is in tab1's hung renderer process.
  ASSERT_NE(nullptr, tab2);
  ASSERT_NE(tab1, tab2);
  ASSERT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess())
      << "New WebContents must be in the same process as the old WebContents, "
      << "so that the new tab doesn't finish loading (what this test is all "
      << "about)";
  ASSERT_EQ(tab1->GetSiteInstance(), tab2->GetSiteInstance())
      << "New WebContents must initially be in the same site instance as the "
      << "old WebContents";

  // Now navigate this tab to a different site. This should wind up in a
  // different renderer process, so it should complete and show up in the task
  // manager.
  tab2->OpenURL(content::OpenURLParams(url3, content::Referrer(), CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticePanel) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Open a new panel to an extension url.
  GURL url(
    "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/french_sentence.html");
  Panel* docked_panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(), url, nullptr, gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);
  docked_panel->Show();

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

  // Create a second, detached panel.
  Panel* detached_panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(), url, nullptr, gfx::Rect(150, 150),
      PanelManager::CREATE_AS_DETACHED);
  detached_panel->ShowInactive();

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      2, MatchExtension("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
                        "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyExtension()));

  // Close the panels and verify that we notice.
  docked_panel->Close();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchExtension("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
                        "french_sentence.html")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));

  detached_panel->Close();
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
      nullptr,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);
  panel->ShowInactive();

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
  Panel* panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      nullptr,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DETACHED);
  panel->Show();

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
  ASSERT_EQ(-1, model()->GetTabId(background_page));
  model()->Kill(background_page);
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
  Panel* panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id()),
      browser()->profile(),
      url,
      nullptr,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DETACHED);
  panel->ShowInactive();

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
  ASSERT_EQ(-1, model()->GetTabId(background_page));

  // Kill the process via the PANEL RESOURCE (not the background page). Verify
  // that both the background page and the panel go away from the task manager.
  int panel_index = FindResourceIndex(MatchExtension(
      "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/"
      "french_sentence.html"));
  ASSERT_NE(-1, panel_index);
  ASSERT_NE(-1, model()->GetTabId(panel_index));
  model()->Kill(panel_index);
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
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_NE(-1, model()->GetTabId(extension_tab));

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_EQ(-1, model()->GetTabId(background_page));
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
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_NE(-1, model()->GetTabId(extension_tab));

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_EQ(-1, model()->GetTabId(background_page));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabChanges) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // "Extension: Packaged App Test" instead of "App: Packaged App Test".
  DisableNewTaskManagerForBrokenTest();

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
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

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
  ASSERT_NE(-1, model()->GetTabId(app_tab));
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
  // TODO(nick, afakhry): Broken on new task manager because we show
  // "Extension: Packaged App Test" instead of "App: Packaged App Test".
  DisableNewTaskManagerForBrokenTest();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

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
  ASSERT_NE(-1, model()->GetTabId(app_tab));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabChanges) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // "Tab: Unmodified" instead of "App: ".
  DisableNewTaskManagerForBrokenTest();

  ShowTaskManager();

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
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
  // TODO(nick, afakhry): This fails on the new task manager (we never
  // reclassify the tab as an app). Remove when fixed.
  DisableNewTaskManagerForBrokenTest();

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL base_url =
      embedded_test_server()->GetURL("/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
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
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL base_url =
      embedded_test_server()->GetURL("/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
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

// Checks that task manager counts a worker thread JS heap size.
// http://crbug.com/241066
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, WebWorkerJSHeapMemory) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(task_manager::browsertest_util::V8_MEMORY);
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "var blob = new Blob([\n"
      "    'mem = new Array(%lu);',\n"
      "    'for (var i = 0; i < mem.length; i += 16)',"
      "    '  mem[i] = i;',\n"
      "    'postMessage(\"okay\");']);\n"
      "blobURL = window.URL.createObjectURL(blob);\n"
      "var worker = new Worker(blobURL);\n"
      "worker.addEventListener('message', function(e) {\n"
      "  window.domAutomationController.send(e.data);\n"  // e.data == "okay"
      "});\n"
      "worker.postMessage('go');\n",
      static_cast<unsigned long>(minimal_heap_size));
  std::string ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_EQ("okay", ok);

  // The worker has allocated objects of at least |minimal_heap_size| bytes.
  // Wait for the heap stats to reflect this.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), task_manager::browsertest_util::V8_MEMORY,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), task_manager::browsertest_util::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
}

// Checks that task manager counts renderer JS heap size.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, JSHeapMemory) {
  ShowTaskManager();
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "mem = new Array(%lu);\n"
      "for (var i = 0; i < mem.length; i += 16)\n"
      "  mem[i] = i;\n"
      "window.domAutomationController.send(\"okay\");\n",
      static_cast<unsigned long>(minimal_heap_size));
  std::string ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_EQ("okay", ok);

  model()->ToggleColumnVisibility(task_manager::browsertest_util::V8_MEMORY);

  // The page's js has allocated objects of at least |minimal_heap_size| bytes.
  // Wait for the heap stats to reflect this.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), task_manager::browsertest_util::V8_MEMORY,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), task_manager::browsertest_util::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
}

// Checks that task manager counts utility process JS heap size.
IN_PROC_BROWSER_TEST_F(TaskManagerUtilityProcessBrowserTest,
                       UtilityJSHeapMemory) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(task_manager::browsertest_util::V8_MEMORY);

  auto proxy_resolver_name =
      l10n_util::GetStringUTF16(IDS_UTILITY_PROCESS_PROXY_RESOLVER_NAME);
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  // The PAC script is trivial, so don't expect a large heap.
  size_t minimal_heap_size = 1024;
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchUtility(proxy_resolver_name),
      task_manager::browsertest_util::V8_MEMORY, minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchUtility(proxy_resolver_name),
      task_manager::browsertest_util::V8_MEMORY_USED, minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyUtility()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchUtility(proxy_resolver_name)));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewDockedWindow) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // a long chrome-devtools:// URL without a prefix (expecting "Tab: *").
  DisableNewTaskManagerForBrokenTest();

  ShowTaskManager();  // Task manager shown BEFORE dev tools window.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewUndockedWindow) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // a long chrome-devtools:// URL without a prefix (expecting "Tab: *").
  DisableNewTaskManagerForBrokenTest();

  ShowTaskManager();  // Task manager shown BEFORE dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldDockedWindow) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // a long chrome-devtools:// URL without a prefix (expecting "Tab: *").
  DisableNewTaskManagerForBrokenTest();

  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldUndockedWindow) {
  // TODO(nick, afakhry): Broken on new task manager because we show
  // a long chrome-devtools:// URL without a prefix (expecting "Tab: *").
  DisableNewTaskManagerForBrokenTest();

  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, KillSubframe) {
  ShowTaskManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  GURL main_url(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(main_url, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
    int subframe_b = FindResourceIndex(MatchSubframe("http://b.com/"));
    ASSERT_NE(-1, subframe_b);
    ASSERT_NE(-1, model()->GetTabId(subframe_b));

    model()->Kill(subframe_b);

    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  }

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  }
}

// Tests what happens when a tab navigates to a site (a.com) that it previously
// has a cross-process subframe into (b.com).
IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, NavigateToSubframeProcess) {
  ShowTaskManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Now navigate to a page on b.com with a simple (same-site) iframe.
  // This should not show any subframe resources in the task manager.
  GURL b_dotcom(
      embedded_test_server()->GetURL("/cross-site/b.com/iframe.html"));

  browser()->OpenURL(content::OpenURLParams(b_dotcom, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  HideTaskManager();
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       NavigateToSiteWithSubframeToOriginalSite) {
  ShowTaskManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Navigate to a page on b.com with a simple (same-site) iframe.
  // This should not show any subframe resources in the task manager.
  GURL b_dotcom(
      embedded_test_server()->GetURL("/cross-site/b.com/iframe.html"));

  browser()->OpenURL(content::OpenURLParams(b_dotcom, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  // Now navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }
}

// Tests what happens when a tab navigates a cross-frame iframe (to b.com)
// back to the site of the parent document (a.com).
IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       CrossSiteIframeBecomesSameSite) {
  ShowTaskManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Navigate the b.com frame back to a.com. It is no longer a cross-site iframe
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      "document.getElementById('frame1').src='/title1.html';"
      "document.title='aac';"));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       LeavePageWithCrossSiteIframes) {
  ShowTaskManager();

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  content::SetupCrossSiteRedirector(embedded_test_server());

  // Navigate the tab to a page on a.com with cross-process subframes.
  GURL a_dotcom_with_iframes(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom_with_iframes,
                                            content::Referrer(), CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Navigate the tab to a page on a.com without cross-process subframes, and
  // the subframe processes should disappear.
  GURL a_dotcom_simple(
      embedded_test_server()->GetURL("/cross-site/a.com/title2.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom_simple,
                                            content::Referrer(), CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  HideTaskManager();
  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}
