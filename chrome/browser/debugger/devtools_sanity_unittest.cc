// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/browser/worker_host/worker_service_observer.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "net/test/test_server.h"

using content::BrowserThread;

namespace {

// Used to block until a dev tools client window's browser is closed.
class BrowserClosedObserver : public content::NotificationObserver {
 public:
  explicit BrowserClosedObserver(Browser* browser) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                   content::Source<Browser>(browser));
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    MessageLoopForUI::current()->Quit();
  }

 private:
  content::NotificationRegistrar registrar_;
  DISALLOW_COPY_AND_ASSIGN(BrowserClosedObserver);
};

// The delay waited in some cases where we don't have a notifications for an
// action we take.
const int kActionDelayMs = 500;

const char kDebuggerTestPage[] = "files/devtools/debugger_test_page.html";
const char kPauseWhenLoadingDevTools[] =
    "files/devtools/pause_when_loading_devtools.html";
const char kPauseWhenScriptIsRunning[] =
    "files/devtools/pause_when_script_is_running.html";
const char kPageWithContentScript[] =
    "files/devtools/page_with_content_script.html";
const char kNavigateBackTestPage[] =
    "files/devtools/navigate_back.html";
const char kChunkedTestPage[] = "chunked";
const char kSlowTestPage[] =
    "chunked?waitBeforeHeaders=100&waitBetweenChunks=100&chunksNumber=2";
const char kSharedWorkerTestPage[] =
    "files/workers/workers_ui_shared_worker.html";
const char kReloadSharedWorkerTestPage[] =
    "files/workers/debug_shared_worker_initialization.html";

void RunTestFuntion(DevToolsWindow* window, const char* test_name) {
  std::string result;

  // At first check that JavaScript part of the front-end is loaded by
  // checking that global variable uiTests exists(it's created after all js
  // files have been loaded) and has runTest method.
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          window->GetRenderViewHost(),
          L"",
          L"window.domAutomationController.send("
          L"'' + (window.uiTests && (typeof uiTests.runTest)));",
          &result));

  if (result == "function") {
    ASSERT_TRUE(
        ui_test_utils::ExecuteJavaScriptAndExtractString(
            window->GetRenderViewHost(),
            L"",
            UTF8ToWide(base::StringPrintf("uiTests.runTest('%s')",
                                          test_name)),
            &result));
    EXPECT_EQ("[OK]", result);
  } else {
    FAIL() << "DevTools front-end is broken.";
  }
}

class DevToolsSanityTest : public InProcessBrowserTest {
 public:
  DevToolsSanityTest()
      : window_(NULL),
        inspected_rvh_(NULL) {
    set_show_window(true);
    EnableDOMAutomation();
  }

 protected:
  void RunTest(const std::string& test_name, const std::string& test_page) {
    OpenDevToolsWindow(test_page);
    RunTestFuntion(window_, test_name.c_str());
    CloseDevToolsWindow();
  }

  void OpenDevToolsWindow(const std::string& test_page) {
    ASSERT_TRUE(test_server()->Start());
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);

    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    inspected_rvh_ = GetInspectedTab()->render_view_host();
    window_ = DevToolsWindow::OpenDevToolsWindow(inspected_rvh_);
    observer.Wait();
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

  DevToolsWindow* window_;
  RenderViewHost* inspected_rvh_;
};

void TimeoutCallback(const std::string& timeout_message) {
  FAIL() << timeout_message;
  MessageLoop::current()->Quit();
}

// Base class for DevTools tests that test devtools functionality for
// extensions and content scripts.
class DevToolsExtensionDebugTest : public DevToolsSanityTest,
                                   public content::NotificationObserver {
 public:
  DevToolsExtensionDebugTest() : DevToolsSanityTest() {
    PathService::Get(chrome::DIR_TEST_DATA, &test_extensions_dir_);
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("devtools");
    test_extensions_dir_ = test_extensions_dir_.AppendASCII("extensions");
  }

 protected:
  // Load an extension from test\data\devtools\extensions\<extension_name>
  void LoadExtension(const char* extension_name) {
    FilePath path = test_extensions_dir_.AppendASCII(extension_name);
    ASSERT_TRUE(LoadExtensionFromPath(path)) << "Failed to load extension.";
  }

 private:
  bool LoadExtensionFromPath(const FilePath& path) {
    ExtensionService* service = browser()->profile()->GetExtensionService();
    size_t num_before = service->extensions()->size();
    {
      content::NotificationRegistrar registrar;
      registrar.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                    content::NotificationService::AllSources());
      base::CancelableCallback timeout(
          base::Bind(&TimeoutCallback, "Extension load timed out."));
      MessageLoop::current()->PostDelayedTask(
          FROM_HERE, timeout.callback(), 4 * 1000);
      extensions::UnpackedInstaller::Create(service)->Load(path);
      ui_test_utils::RunMessageLoop();
      timeout.Cancel();
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

    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                  content::NotificationService::AllSources());
    base::CancelableCallback timeout(
        base::Bind(&TimeoutCallback, "Extension host load timed out."));
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE, timeout.callback(), 4 * 1000);

    ExtensionProcessManager* manager =
          browser()->profile()->GetExtensionProcessManager();
    for (ExtensionProcessManager::const_iterator iter = manager->begin();
         iter != manager->end();) {
      if ((*iter)->did_stop_loading())
        ++iter;
      else
        ui_test_utils::RunMessageLoop();
    }

    timeout.Cancel();
    return true;
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    switch (type) {
      case chrome::NOTIFICATION_EXTENSION_LOADED:
      case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
        MessageLoopForUI::current()->Quit();
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  FilePath test_extensions_dir_;
};


class WorkerDevToolsSanityTest : public InProcessBrowserTest {
 public:
  WorkerDevToolsSanityTest() : window_(NULL) {
    set_show_window(true);
    EnableDOMAutomation();
  }

 protected:
  struct WorkerData : public base::RefCountedThreadSafe<WorkerData> {
    WorkerData() : worker_process_id(0), worker_route_id(0) {}
    int worker_process_id;
    int worker_route_id;
  };

  class WorkerCreationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerCreationObserver(WorkerData* worker_data)
        : worker_data_(worker_data) {
    }

   private:
    virtual ~WorkerCreationObserver() {}

    virtual void WorkerCreated (
        WorkerProcessHost* process,
        const WorkerProcessHost::WorkerInstance& instance) OVERRIDE {
      worker_data_->worker_process_id = process->id();
      worker_data_->worker_route_id = instance.worker_route_id();
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          new MessageLoop::QuitTask);
      delete this;
    }
    virtual void WorkerDestroyed(
        WorkerProcessHost*,
        int worker_route_id) OVERRIDE {}
    virtual void WorkerContextStarted(
        WorkerProcessHost*,
        int worker_route_id) OVERRIDE {}
    scoped_refptr<WorkerData> worker_data_;
  };

  class WorkerTerminationObserver : public WorkerServiceObserver {
   public:
    explicit WorkerTerminationObserver(WorkerData* worker_data)
        : worker_data_(worker_data) {
    }

   private:
    virtual ~WorkerTerminationObserver() {}

    virtual void WorkerCreated (
        WorkerProcessHost* process,
        const WorkerProcessHost::WorkerInstance& instance) OVERRIDE {}
    virtual void WorkerDestroyed(
        WorkerProcessHost* process,
        int worker_route_id) OVERRIDE {
      ASSERT_EQ(worker_data_->worker_process_id, process->id());
      ASSERT_EQ(worker_data_->worker_route_id, worker_route_id);
      WorkerService::GetInstance()->RemoveObserver(this);
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
          new MessageLoop::QuitTask);
      delete this;
    }
    virtual void WorkerContextStarted(
        WorkerProcessHost*,
        int worker_route_id) OVERRIDE {}
    scoped_refptr<WorkerData> worker_data_;
  };

  void RunTest(const char* test_name, const char* test_page) {
    ASSERT_TRUE(test_server()->Start());
    GURL url = test_server()->GetURL(test_page);
    ui_test_utils::NavigateToURL(browser(), url);

    scoped_refptr<WorkerData> worker_data = WaitForFirstSharedWorker();
    OpenDevToolsWindowForSharedWorker(worker_data.get());
    RunTestFuntion(window_, test_name);
    CloseDevToolsWindow();
  }

  static void TerminateWorkerOnIOThread(
      scoped_refptr<WorkerData> worker_data) {
    for (BrowserChildProcessHost::Iterator iter(
             ChildProcessInfo::WORKER_PROCESS);
         !iter.Done(); ++iter) {
      if (iter->id() == worker_data->worker_process_id) {
        WorkerProcessHost* host = static_cast<WorkerProcessHost*>(*iter);
        host->TerminateWorker(worker_data->worker_route_id);
        WorkerService::GetInstance()->AddObserver(
            new WorkerTerminationObserver(worker_data));
        return;
      }
    }
    FAIL() << "Failed to terminate worker.\n";
  }

  static void TerminateWorker(scoped_refptr<WorkerData> worker_data) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&TerminateWorkerOnIOThread, worker_data));
    ui_test_utils::RunMessageLoop();
  }

  static void WaitForFirstSharedWorkerOnIOThread(
      scoped_refptr<WorkerData> worker_data) {
    BrowserChildProcessHost::Iterator iter(ChildProcessInfo::WORKER_PROCESS);
    for (; !iter.Done(); ++iter) {
      WorkerProcessHost* worker = static_cast<WorkerProcessHost*>(*iter);
      const WorkerProcessHost::Instances& instances = worker->instances();
      for (WorkerProcessHost::Instances::const_iterator i = instances.begin();
           i != instances.end(); ++i) {

        worker_data->worker_process_id = worker->id();
        worker_data->worker_route_id = i->worker_route_id();
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
            new MessageLoop::QuitTask);
        return;
      }
    }

    WorkerService::GetInstance()->AddObserver(
        new WorkerCreationObserver(worker_data.get()));
  }

  static scoped_refptr<WorkerData> WaitForFirstSharedWorker() {
    scoped_refptr<WorkerData> worker_data(new WorkerData());
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&WaitForFirstSharedWorkerOnIOThread, worker_data));
    ui_test_utils::RunMessageLoop();
    return worker_data;
  }

  void OpenDevToolsWindowForSharedWorker(WorkerData* worker_data) {
    Profile* profile = browser()->profile();
    window_ = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    window_->Show(DEVTOOLS_TOGGLE_ACTION_NONE);
    DevToolsAgentHost* agent_host =
        WorkerDevToolsManager::GetDevToolsAgentHostForWorker(
            worker_data->worker_process_id,
            worker_data->worker_route_id);
    DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host,
        window_);
    RenderViewHost* client_rvh = window_->GetRenderViewHost();
    TabContents* client_contents = client_rvh->delegate()->GetAsTabContents();
    if (client_contents->IsLoading()) {
      ui_test_utils::WindowedNotificationObserver observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<NavigationController>(
              &client_contents->controller()));
      observer.Wait();
    }
  }

  void CloseDevToolsWindow() {
    Browser* browser = window_->browser();
    browser->CloseAllTabs();
    BrowserClosedObserver close_observer(browser);
  }

  DevToolsWindow* window_;
};


// Tests scripts panel showing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestShowScriptsTab) {
  RunTest("testShowScriptsTab", kDebuggerTestPage);
}

// Tests that scripts tab is populated with inspected scripts even if it
// hadn't been shown by the moment inspected paged refreshed.
// @see http://crbug.com/26312
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest,
                       FAILS_TestScriptsTabIsPopulatedOnInspectedPageRefresh) {
  // Clear inspector settings to ensure that Elements will be
  // current panel when DevTools window is open.
  content::GetContentClient()->browser()->ClearInspectorSettings(
      GetInspectedTab()->render_view_host());
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

// Tests network timing.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkTiming) {
  RunTest("testNetworkTiming", kSlowTestPage);
}

// Tests network size.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSize) {
  RunTest("testNetworkSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkSyncSize) {
  RunTest("testNetworkSyncSize", kChunkedTestPage);
}

// Tests raw headers text.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestNetworkRawHeadersText) {
  RunTest("testNetworkRawHeadersText", kChunkedTestPage);
}

// Tests that console messages are not duplicated on navigation back.
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestConsoleOnNavigateBack) {
  RunTest("testConsoleOnNavigateBack", kNavigateBackTestPage);
}

#if defined(OS_LINUX)
// http://crbug.com/103539
#define TestReattachAfterCrash FLAKY_TestReattachAfterCrash
#endif
// Tests that inspector will reattach to inspected page when it is reloaded
// after a crash. See http://crbug.com/101952
IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestReattachAfterCrash) {
  OpenDevToolsWindow(kDebuggerTestPage);

  ui_test_utils::CrashTab(GetInspectedTab());
  ui_test_utils::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContentsWrapper()->controller()));
  browser()->Reload(CURRENT_TAB);
  observer.Wait();

  RunTestFuntion(window_, "testReattachAfterCrash");
  CloseDevToolsWindow();
}

IN_PROC_BROWSER_TEST_F(DevToolsSanityTest, TestPageWithNoJavaScript) {
  OpenDevToolsWindow("about:blank");
  std::string result;
  ASSERT_TRUE(
      ui_test_utils::ExecuteJavaScriptAndExtractString(
          window_->GetRenderViewHost(),
          L"",
          L"window.domAutomationController.send("
          L"'' + (window.uiTests && (typeof uiTests.runTest)));",
          &result));
  ASSERT_EQ("function", result) << "DevTools front-end is broken.";
  CloseDevToolsWindow();
}

#if defined(OS_MACOSX)
#define MAYBE_InspectSharedWorker DISABLED_InspectSharedWorker
#else
#define MAYBE_InspectSharedWorker InspectSharedWorker
#endif
// Flakily fails with 25s timeout: http://crbug.com/89845
IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest, MAYBE_InspectSharedWorker) {
  RunTest("testSharedWorker", kSharedWorkerTestPage);
}

// http://crbug.com/100538
#if defined(OS_MACOSX)
#define MAYBE_PauseInSharedWorkerInitialization DISABLED_PauseInSharedWorkerInitialization
#elif defined(OS_WIN)
#define MAYBE_PauseInSharedWorkerInitialization FLAKY_PauseInSharedWorkerInitialization
#else
#define MAYBE_PauseInSharedWorkerInitialization PauseInSharedWorkerInitialization
#endif
// See http://crbug.com/100538
IN_PROC_BROWSER_TEST_F(WorkerDevToolsSanityTest,
                       MAYBE_PauseInSharedWorkerInitialization) {
    ASSERT_TRUE(test_server()->Start());
    GURL url = test_server()->GetURL(kReloadSharedWorkerTestPage);
    ui_test_utils::NavigateToURL(browser(), url);

    scoped_refptr<WorkerData> worker_data = WaitForFirstSharedWorker();
    OpenDevToolsWindowForSharedWorker(worker_data.get());

    TerminateWorker(worker_data);

    // Reload page to restart the worker.
    ui_test_utils::NavigateToURL(browser(), url);

    // Wait until worker script is paused on the debugger statement.
    RunTestFuntion(window_, "testPauseInSharedWorkerInitialization");
    CloseDevToolsWindow();
}

}  // namespace
