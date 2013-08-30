// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

// http://crbug.com/31663
// TODO(linux_aura) http://crbug.com/163931
#if !(defined(OS_WIN) && defined(USE_AURA)) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA))

using content::WebContents;

// On Linux this is crashing intermittently http://crbug/84719
// In some environments this test fails about 1/6 http://crbug/84850
#if defined(OS_LINUX)
#define MAYBE_KillExtension DISABLED_KillExtension
#else
#define MAYBE_KillExtension KillExtension
#endif

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

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();

    EXPECT_EQ(0, model()->ResourceCount());

    // Show the task manager. This populates the model, and helps with debugging
    // (you see the task manager).
    chrome::ShowTaskManager(browser());

    // New Tab Page.
    TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
  }

  void Refresh() {
    model()->Refresh();
  }

  int GetUpdateTimeMs() {
    return TaskManagerModel::kUpdateTimeMs;
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not prelaunch the GPU process and disable accelerated compositing
    // for these tests as the GPU process will show up in task manager but
    // whether it appears before or after the new tab renderer process is not
    // well defined.
    command_line->AppendSwitch(switches::kDisableGpuProcessPrelaunch);
    command_line->AppendSwitch(switches::kDisableAcceleratedCompositing);
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
  // Showing task manager handled by SetUp.
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();
  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle1File)));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Check that the last entry is a tab contents resource whose title starts
  // starts with "Tab:".
  ASSERT_TRUE(model()->GetResourceWebContents(resource_count) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count), prefix,
                         true));

  // Close the tab and verify that we notice.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
}

#if defined(USE_ASH)
// This test fails on Ash because task manager treats view type
// Panels differently for Ash.
#define MAYBE_NoticePanelChanges DISABLED_NoticePanelChanges
#else
#define MAYBE_NoticePanelChanges NoticePanelChanges
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_NoticePanelChanges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, the New Tab Page and Extension background page.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Open a new panel to an extension url and make sure we notice that.
  GURL url(
    "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/french_sentence.html");
  Panel* panel = PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id_),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(3);

  // Check that the fourth entry is a resource with the panel's web contents
  // and whose title starts with "Extension:".
  ASSERT_EQ(panel->GetWebContents(), model()->GetResourceWebContents(3));
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_EXTENSION_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(3), prefix, true));

  // Close the panel and verify that we notice.
  panel->Close();
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
}

#if defined(USE_ASH) || defined(OS_WIN)
// This test fails on Ash because task manager treats view type
// Panels differently for Ash.
// This test also fails on Windows, win_rel trybot. http://crbug.com/166322
#define MAYBE_KillPanelExtension DISABLED_KillPanelExtension
#else
#define MAYBE_KillPanelExtension KillPanelExtension
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_KillPanelExtension) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, the New Tab Page and Extension background page.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Open a new panel to an extension url and make sure we notice that.
  GURL url(
    "chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/french_sentence.html");
  PanelManager::GetInstance()->CreatePanel(
      web_app::GenerateApplicationNameFromExtensionId(
          last_loaded_extension_id_),
      browser()->profile(),
      url,
      gfx::Rect(300, 400),
      PanelManager::CREATE_AS_DOCKED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(3);

  // Kill the panel extension process and verify that it disappears from the
  // model along with its panel.
  ASSERT_TRUE(model()->IsBackgroundResource(resource_count));
  TaskManager::GetInstance()->KillProcess(resource_count);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTabs) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, Extension background page, and the New Tab Page.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Open a new tab to an extension URL and make sure we notice that.
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(3);

  // Check that the third entry (background) is an extension resource whose
  // title starts with "Extension:".
  ASSERT_EQ(task_manager::Resource::EXTENSION, model()->GetResourceType(
      resource_count));
  ASSERT_TRUE(model()->GetResourceWebContents(resource_count) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(resource_count) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_EXTENSION_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         prefix, true));

  // Check that the fourth entry (page.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "Extension:".
  ASSERT_EQ(task_manager::Resource::EXTENSION, model()->GetResourceType(
      resource_count + 1));
  ASSERT_TRUE(model()->GetResourceWebContents(resource_count + 1) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(resource_count + 1) != NULL);
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count + 1),
                         prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabs) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);

  // New Tab Page.
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "App:".
  ASSERT_EQ(task_manager::Resource::EXTENSION, model()->GetResourceType(
      resource_count));
  ASSERT_TRUE(model()->GetResourceWebContents(resource_count) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(resource_count) == extension);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_APP_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabs) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();

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

  // Force the TaskManager to query the title.
  Refresh();

  // Check that the third entry's title starts with "Tab:".
  string16 tab_prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         tab_prefix, true));

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));
  // Force the TaskManager to query the title.
  Refresh();
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         tab_prefix, true));

  // Now reload and check that the last entry's title now starts with "App:".
  ui_test_utils::NavigateToURL(browser(), url);
  // Force the TaskManager to query the title.
  Refresh();
  string16 app_prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_APP_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         app_prefix, true));

  // Disable extension and reload page.
  DisableExtension(last_loaded_extension_id_);
  ui_test_utils::NavigateToURL(browser(), url);

  // Force the TaskManager to query the title.
  Refresh();

  // The third entry's title should be back to a normal tab.
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(resource_count),
                         tab_prefix, true));
}

// Disabled, http://crbug.com/66957.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_KillExtensionAndReload) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  // Kill the extension process and make sure we notice it.
  TaskManager::GetInstance()->KillProcess(2);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(1);

  // Reload the extension using the "crashed extension" infobar while the task
  // manager is still visible. Make sure we don't crash and the extension
  // gets reloaded and noticed in the task manager.
  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_EQ(1U, infobar_service->infobar_count());
  ConfirmInfoBarDelegate* delegate =
      infobar_service->infobar_at(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  delegate->Accept();
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(3);
}

#if defined(OS_WIN)
// http://crbug.com/93158.
#define MAYBE_ReloadExtension DISABLED_ReloadExtension
#else
#define MAYBE_ReloadExtension ReloadExtension
#endif

// Regression test for http://crbug.com/18693.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_ReloadExtension) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();
  LOG(INFO) << "loading extension";
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  LOG(INFO) << "waiting for resource change";
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(resource_count) != NULL);

  const extensions::Extension* extension = model()->GetResourceExtension(
      resource_count);
  ASSERT_TRUE(extension != NULL);

  // Reload the extension a few times and make sure our resource count
  // doesn't increase.
  LOG(INFO) << "First extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);
  extension = model()->GetResourceExtension(resource_count);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Second extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);
  extension = model()->GetResourceExtension(resource_count);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Third extension reload";
  ReloadExtension(extension->id());
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);
}

// Crashy, http://crbug.com/42301.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_PopulateWebCacheFields) {
  int resource_count = TaskManager::GetInstance()->model()->ResourceCount();

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle1File)));
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);

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
  GURL url(ui_test_utils::GetTestUrl(base::FilePath(
      base::FilePath::kCurrentDirectory), base::FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);
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

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeInTabDevToolsWindow) {
  DevToolsWindow* dev_tools = DevToolsWindow::ToggleDevToolsWindow(
      model()->GetResourceWebContents(1)->GetRenderViewHost(),
      true,
      DEVTOOLS_TOGGLE_ACTION_INSPECT);
  // Dock side bottom should be the default.
  ASSERT_EQ(DEVTOOLS_DOCK_SIDE_BOTTOM, dev_tools->dock_side());
  TaskManagerBrowserTestUtil::WaitForWebResourceChange(2);
}

#endif
