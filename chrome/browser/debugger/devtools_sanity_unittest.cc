// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// Used to block until a dev tools client window's browser is closed.
class BrowserClosedObserver : public NotificationObserver {
 public:
  explicit BrowserClosedObserver(Browser* browser) {
    registrar_.Add(this, NotificationType::BROWSER_CLOSED,
                   Source<Browser>(browser));
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    MessageLoopForUI::current()->Quit();
  }

 private:
  NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(BrowserClosedObserver);
};

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const char kConsoleTestPage[] = "files/devtools/console_test_page.html";
const char kDebuggerTestPage[] = "files/devtools/debugger_test_page.html";
const char kEvalTestPage[] = "files/devtools/eval_test_page.html";
const char kJsPage[] = "files/devtools/js_page.html";
const char kHeapProfilerPage[] = "files/devtools/heap_profiler.html";
const char kPauseOnExceptionTestPage[] =
    "files/devtools/pause_on_exception.html";
const char kPauseWhenLoadingDevTools[] =
    "files/devtools/pause_when_loading_devtools.html";
const char kPauseWhenScriptIsRunning[] =
    "files/devtools/pause_when_script_is_running.html";
const char kResourceContentLengthTestPage[] = "files/devtools/image.html";
const char kResourceTestPage[] = "files/devtools/resource_test_page.html";
const char kSimplePage[] = "files/devtools/simple_page.html";
const char kSyntaxErrorTestPage[] =
    "files/devtools/script_syntax_error.html";
const char kDebuggerStepTestPage[] =
    "files/devtools/debugger_step.html";
const char kDebuggerClosurePage[] =
    "files/devtools/debugger_closure.html";
const char kDebuggerIntrinsicPropertiesPage[] =
    "files/devtools/debugger_intrinsic_properties.html";
const char kCompletionOnPause[] =
    "files/devtools/completion_on_pause.html";
const char kPageWithContentScript[] =
    "files/devtools/page_with_content_script.html";


class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

 protected:
  void RunTest(const std::string& test_name, const std::string& test_page) {
    OpenDevToolsWindow(test_page);
    std::string result;

    // At first check that JavaScript part of the front-end is loaded by
    // checking that global variable uiTests exists(it's created after all js
    // files have been loaded) and has runTest method.
    ASSERT_TRUE(
        ui_test_utils::ExecuteJavaScriptAndExtractString(
            client_contents_->render_view_host(),
            L"",
            L"window.domAutomationController.send("
            L"'' + (window.uiTests && (typeof uiTests.runTest)));",
            &result));

    if (result == "function") {
      ASSERT_TRUE(
          ui_test_utils::ExecuteJavaScriptAndExtractString(
              client_contents_->render_view_host(),
              L"",
              UTF8ToWide(StringPrintf("uiTests.runTest('%s')",
                                      test_name.c_str())),
              &result));
      EXPECT_EQ("[OK]", result);
    } else {
      FAIL() << "DevTools front-end is broken.";
    }
    CloseDevToolsWindow();
  }

  void OpenDevToolsWindow(const std::string& test_page) {
    HTTPTestServer* server = StartHTTPServer();
    GURL url = server->TestServerPage(test_page);
    ui_test_utils::NavigateToURL(browser(), url);

    inspected_rvh_ = GetInspectedTab()->render_view_host();
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    devtools_manager->OpenDevToolsWindow(inspected_rvh_);

    DevToolsClientHost* client_host =
        devtools_manager->GetDevToolsClientHostFor(inspected_rvh_);
    window_ = client_host->AsDevToolsWindow();
    RenderViewHost* client_rvh = window_->GetRenderViewHost();
    client_contents_ = client_rvh->delegate()->GetAsTabContents();
    ui_test_utils::WaitForNavigation(&client_contents_->controller());
  }

  TabContents* GetInspectedTab() {
    return browser()->GetTabContentsAt(0);
  }

  void CloseDevToolsWindow() {
    DevToolsManager* devtools_manager = DevToolsManager::GetInstance();
    // UnregisterDevToolsClientHostFor may destroy window_ so store the browser
    // first.
    Browser* browser = window_->browser();
    devtools_manager->UnregisterDevToolsClientHostFor(inspected_rvh_);

    // Wait only when DevToolsWindow has a browser. For docked DevTools, this
    // is NULL and we skip the wait.
    if (browser)
      BrowserClosedObserver close_observer(browser);
  }

  TabContents* client_contents_;
  DevToolsWindow* window_;
  RenderViewHost* inspected_rvh_;
};


class CancelableQuitTask : public Task {
 public:
  explicit CancelableQuitTask(const std::string& timeout_message)
      : timeout_message_(timeout_message),
        cancelled_(false) {
  }

  void cancel() {
    cancelled_ = true;
  }

  virtual void Run() {
    if (cancelled_) {
      return;
    }
    FAIL() << timeout_message_;
    MessageLoop::current()->Quit();
  }

 private:
  std::string timeout_message_;
  bool cancelled_;
};


// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionDebugTest : public DevToolsSanityTest,
                                   public NotificationObserver {
 public:
  DevToolsExtensionDebugTest() : DevToolsSanityTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_extensions_dir_);
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("devtools");
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("extensions");
  }

 protected:
  // Load an extention from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

 private:
  bool LoadExtensionFromPath(const FilePath& path) {
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    size_t num_before = service->extensions()->size();
    {
      NotificationRegistrar registrar;
      registrar.Add(this, NotificationType::EXTENSION_LOADED,
                    NotificationService::AllSources());
      CancelableQuitTask* delayed_quit =
          new CancelableQuitTask("Extension load timed out.");
      MessageLoop::current()->PostDelayedTask(FROM_HERE, delayed_quit,
          4*1000);
      service->LoadExtension(path);
      ui_test_utils::RunMessageLoop();
      delayed_quit->cancel();
    }
    size_t num_after = service->extensions()->size();
    if (num_after != (num_before + 1))
      return false;

    return WaitForExtensionHostsToLoad();
  }

  bool WaitForExtensionHostsToLoad() {
    // Wait for all the extension hosts that exist to finish loading.
    // NOTE: This assumes that the extension host list is not changing while
    // this method is running.

    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                  NotificationService::AllSources());
    CancelableQuitTask* delayed_quit =
        new CancelableQuitTask("Extension host load timed out.");
    MessageLoop::current()->PostDelayedTask(FROM_HERE, delayed_quit,
        4*1000);

    ExtensionProcessManager* manager =
          browser()->profile()->GetExtensionProcessManager();
    for (ExtensionProcessManager::const_iterator iter = manager->begin();
         iter != manager->end();) {
      if ((*iter)->did_stop_loading())
        ++iter;
      else
        ui_test_utils::RunMessageLoop();
    }

    delayed_quit->cancel();
    return true;
  }

  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::EXTENSION_LOADED:
      case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
        MessageLoopForUI::current()->Quit();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  FilePath test_extensions_dir_;
};

// WebInspector opens.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestHostIsPresent) {
  RunTest("testHostIsPresent", kSimplePage);
}

// Tests elements panel basics.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestElementsTreeRoot) {
  RunTest("testElementsTreeRoot", kSimplePage);
}

// Tests main resource load.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestMainResource) {
  RunTest("testMainResource", kSimplePage);
}

// Tests resources panel enabling.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestEnableResourcesTab) {
  RunTest("testEnableResourcesTab", kSimplePage);
}

// Fails after WebKit roll 59365:59477, http://crbug.com/44202.
#if defined(OS_LINUX)
#define MAYBE_TestResourceContentLength FLAKY_TestResourceContentLength
#else
#define MAYBE_TestResourceContentLength TestResourceContentLength
#endif  // defined(OS_LINUX)

// Tests resources have correct sizes.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestResourceContentLength) {
  RunTest("testResourceContentLength", kResourceContentLengthTestPage);
}

// Tests resource headers.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestResourceHeaders) {
  RunTest("testResourceHeaders", kResourceTestPage);
}

// Tests cached resource mime type.
// @see http://crbug.com/27364
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestCachedResourceMimeType) {
  RunTest("testCachedResourceMimeType", kResourceTestPage);
}

// Tests profiler panel.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestProfilerTab) {
  RunTest("testProfilerTab", kJsPage);
}

// Tests heap profiler.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestHeapProfiler) {
  RunTest("testHeapProfiler", kHeapProfilerPage);
}

// Tests scripts panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests that scripts tab is populated with inspected scripts even if it
// hadn't been shown by the moment inspected paged refreshed.
// @see http://crbug.com/26312
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestScriptsTabIsPopulatedOnInspectedPageRefresh) {
  // Clear inspector settings to ensure that Elements will be
  // current panel when DevTools window is open.
  GetInspectedTab()->render_view_host()->delegate()->ClearInspectorSettings();
  RunTest("testScriptsTabIsPopulatedOnInspectedPageRefresh",
          kDebuggerTestPage);
}

// Tests that a content script is in the scripts list.
// This test is disabled, see bug 28961.
IN_PROC_BROWSER_TEST_F(DevToolsExtensionDebugTest,
                       TestContentScriptIsPresent) {
  LoadExtension("simple_content_script");
  RunTest("testContentScriptIsPresent", kPageWithContentScript);
}

// Tests that scripts are not duplicated after Scripts Panel switch.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       TestNoScriptDuplicatesOnPanelSwitch) {
  RunTest("testNoScriptDuplicatesOnPanelSwitch", kDebuggerTestPage);
}

// Tests set breakpoint.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestSetBreakpoint) {
  RunTest("testSetBreakpoint", kDebuggerTestPage);
}

// Tests pause on exception.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPauseOnException) {
  RunTest("testPauseOnException", kPauseOnExceptionTestPage);
}

// Tests that debugger works correctly if pause event occurs when DevTools
// frontend is being loaded.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPauseWhenLoadingDevTools) {
  RunTest("testPauseWhenLoadingDevTools", kPauseWhenLoadingDevTools);
}

// Tests that pressing 'Pause' will pause script execution if the script
// is already running.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPauseWhenScriptIsRunning) {
  RunTest("testPauseWhenScriptIsRunning", kPauseWhenScriptIsRunning);
}

// Tests eval on call frame.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestEvalOnCallFrame) {
  RunTest("testEvalOnCallFrame", kDebuggerTestPage);
}

#if defined(OS_WIN) || defined(OS_LINUX)
// Sometimes it times out.
// See http://crbug.com/45080
// See http://crbug.com/46299
#define MAYBE_TestStepOver FLAKY_TestStepOver
#else
#define MAYBE_TestStepOver TestStepOver
#endif
// Tests step over functionality in the debugger.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestStepOver) {
  RunTest("testStepOver", kDebuggerStepTestPage);
}

#if defined(OS_WIN) || defined(OS_LINUX)
// Sometimes it times out.
// See http://crbug.com/45080
// See http://crbug.com/46299
#define MAYBE_TestStepOut FLAKY_TestStepOut
#elif defined(OS_CHROMEOS)
// See http://crbug.com/43479
#define MAYBE_TestStepOut FLAKY_TestStepOut
#else
#define MAYBE_TestStepOut TestStepOut
#endif
// Tests step out functionality in the debugger.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestStepOut) {
  RunTest("testStepOut", kDebuggerStepTestPage);
}

#if defined(OS_WIN) || defined(OS_LINUX)
// Sometimes it times out.
// See http://crbug.com/45080
// See http://crbug.com/46299
#define MAYBE_TestStepIn FLAKY_TestStepIn
#elif defined(OS_CHROMEOS)
// See http://crbug.com/43479
#define MAYBE_TestStepIn FLAKY_TestStepIn
#else
#define MAYBE_TestStepIn TestStepIn
#endif
// Disable TestStepIn completely while test expectations are not updated
// in Webkit.  See http://crbug.com/46235
#undef MAYBE_TestStepIn
#define MAYBE_TestStepIn DISABLED_TestStepIn

// Tests step in functionality in the debugger.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, MAYBE_TestStepIn) {
  RunTest("testStepIn", kDebuggerStepTestPage);
}

// Tests that scope can be expanded and contains expected variables.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestExpandScope) {
  RunTest("testExpandScope", kDebuggerClosurePage);
}

// Tests that intrinsic properties(__proto__, prototype, constructor) are
// present.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestDebugIntrinsicProperties) {
  RunTest("testDebugIntrinsicProperties", kDebuggerIntrinsicPropertiesPage);
}

// Tests that execution continues automatically when there is a syntax error in
// script and DevTools are open.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestAutoContinueOnSyntaxError) {
  RunTest("testAutoContinueOnSyntaxError", kSyntaxErrorTestPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestCompletionOnPause) {
  RunTest("testCompletionOnPause", kCompletionOnPause);
}

// Tests that 'Pause' button works for eval.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, DISABLED_TestPauseInEval) {
  RunTest("testPauseInEval", kDebuggerTestPage);
}

// Tests console eval.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestConsoleEval) {
  RunTest("testConsoleEval", kEvalTestPage);
}

// Tests console log.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestConsoleLog) {
  RunTest("testConsoleLog", kConsoleTestPage);
}

// Tests eval global values.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestEvalGlobal) {
  RunTest("testEvalGlobal", kEvalTestPage);
}

// Test that Storage panel can be shown.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestShowStoragePanel) {
  RunTest("testShowStoragePanel", kDebuggerTestPage);
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestMessageLoopReentrant) {
  RunTest("testMessageLoopReentrant", kDebuggerTestPage);
}

}  // namespace
