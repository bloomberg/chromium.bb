// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/url_request/url_request_unittest.h"

static const char kTestCompleteCookie[] = "status";
static const char kTestCompleteSuccess[] = "OK";

class WorkerTest : public UILayoutTest {
 protected:
  virtual ~WorkerTest() { }

  void RunTest(const std::wstring& test_case) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    GURL url = GetTestUrl(L"workers", test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, action_max_timeout_ms());
    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }

  void RunIncognitoTest(const std::wstring& test_case) {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());

    // Open an Incognito window.
    ASSERT_TRUE(browser->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
    scoped_refptr<BrowserProxy> incognito(automation()->GetBrowserWindow(1));
    ASSERT_TRUE(incognito.get());
    int window_count;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(2, window_count);

    scoped_refptr<TabProxy> tab(incognito->GetTab(0));
    ASSERT_TRUE(tab.get());

    GURL url = GetTestUrl(L"workers", test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, action_max_timeout_ms());

    // Close the incognito window
    ASSERT_TRUE(incognito->RunCommand(IDC_CLOSE_WINDOW));
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);

    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }

  bool WaitForProcessCountToBe(int tabs, int workers) {
    // The 1 is for the browser process.
    int number_of_processes = 1 + workers +
        (UITest::in_process_renderer() ? 0 : tabs);
#if defined(OS_LINUX)
    // On Linux, we also have a zygote process and a sandbox host process.
    number_of_processes += 2;
#endif

    int cur_process_count;
    for (int i = 0; i < 10; ++i) {
      cur_process_count = GetBrowserProcessCount();
      if (cur_process_count == number_of_processes)
        return true;

      PlatformThread::Sleep(sleep_timeout_ms() / 10);
    }

    EXPECT_EQ(number_of_processes, cur_process_count);
    return false;
  }

  void RunWorkerFastLayoutTest(const std::string& test_case_file_name) {
    FilePath fast_test_dir;
    fast_test_dir = fast_test_dir.AppendASCII("LayoutTests");
    fast_test_dir = fast_test_dir.AppendASCII("fast");

    FilePath worker_test_dir;
    worker_test_dir = worker_test_dir.AppendASCII("workers");
    InitializeForLayoutTest(fast_test_dir, worker_test_dir, kNoHttpPort);

    // Worker tests also rely on common files in js/resources.
    FilePath js_dir = fast_test_dir.AppendASCII("js");
    FilePath resource_dir;
    resource_dir = resource_dir.AppendASCII("resources");
    AddResourceForLayoutTest(js_dir, resource_dir);

    printf ("Test: %s\n", test_case_file_name.c_str());
    RunLayoutTest(test_case_file_name, kNoHttpPort);

    // Navigate to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    GURL about_url(std::string("about:blank"));
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(about_url));
  }

  bool NavigateAndWaitForAuth(TabProxy* tab, const GURL& url) {
    // Pass a large number of navigations to tell the tab to block until an auth
    // dialog pops up.
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED,
              tab->NavigateToURLBlockUntilNavigationsComplete(url, 100));
    return tab->NeedsAuth();
  }
};


TEST_F(WorkerTest, SingleWorker) {
  RunTest(L"single_worker.html");
}

#if defined(OS_LINUX)
// MultipleWorkers times out (process hangs, does not exit) occasionally.
// http://crbug.com/30353
#define MultipleWorkers DISABLED_MultipleWorkers
#endif

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(L"multi_worker.html");
}

TEST_F(WorkerTest, SingleSharedWorker) {
  RunTest(L"single_worker.html?shared=true");
}

TEST_F(WorkerTest, MultipleSharedWorkers) {
  RunTest(L"multi_worker.html?shared=true");
}

#if defined(OS_LINUX)
#define IncognitoSharedWorkers FLAKY_IncognitoSharedWorkers
#endif

// Incognito windows should not share workers with non-incognito windows
TEST_F(WorkerTest, IncognitoSharedWorkers) {
  // Load a non-incognito tab and have it create a shared worker
  RunTest(L"incognito_worker.html");
  // Incognito worker should not share with non-incognito
  RunIncognitoTest(L"incognito_worker.html");
}

const wchar_t kDocRoot[] = L"chrome/test/data/workers";

// http://crbug.com/36630 termination issues, disabled on all platforms.
// http://crbug.com/33344 - NavigateAndWaitForAuth times out on the Windows
// build bots.
// Make sure that auth dialog is displayed from worker context.
TEST_F(WorkerTest, DISABLED_WorkerHttpAuth) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url = server->TestServerPage("files/worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
}

// http://crbug.com/36630 termination issues, disabled on all platforms.
// http://crbug.com/33344 - NavigateAndWaitForAuth times out on the Windows
// build bots.
// Make sure that auth dialog is displayed from shared worker context.
TEST_F(WorkerTest, DISABLED_SharedWorkerHttpAuth) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url = server->TestServerPage("files/shared_worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
  // TODO(atwilson): Add support to automation framework to test for auth
  // dialogs displayed by non-navigating tabs.
}

TEST_F(WorkerTest, StressJSExecution) {
  RunWorkerFastLayoutTest("stress-js-execution.html");
}

TEST_F(WorkerTest, UseMachineStack) {
  RunWorkerFastLayoutTest("use-machine-stack.html");
}

TEST_F(WorkerTest, WorkerCall) {
  RunWorkerFastLayoutTest("worker-call.html");
}

// Crashy, http://crbug.com/35965.
#if defined(OS_LINUX) || defined(OS_MACOSX)
#define FLAKY_WorkerClonePort DISABLED_WorkerClonePort
#endif

// Flaky, http://crbug.com/36555.
TEST_F(WorkerTest, FLAKY_WorkerClonePort) {
  RunWorkerFastLayoutTest("worker-cloneport.html");
}

// Hangs. http://crbug.com/36630
TEST_F(WorkerTest, DISABLED_WorkerCloseFast) {
  RunWorkerFastLayoutTest("worker-close.html");
}

TEST_F(WorkerTest, WorkerConstructor) {
  RunWorkerFastLayoutTest("worker-constructor.html");
}

TEST_F(WorkerTest, WorkerContextGc) {
  RunWorkerFastLayoutTest("worker-context-gc.html");
}

// All kinds of crashes on Linux http://crbug.com/22898
#if defined(OS_LINUX)
#define WorkerContextMultiPort DISABLED_WorkerContextMultiPort
#endif

TEST_F(WorkerTest, WorkerContextMultiPort) {
  RunWorkerFastLayoutTest("worker-context-multi-port.html");
}

TEST_F(WorkerTest, WorkerEventListener) {
  RunWorkerFastLayoutTest("worker-event-listener.html");
}

TEST_F(WorkerTest, WorkerGC) {
  RunWorkerFastLayoutTest("worker-gc.html");
}

// worker-lifecycle.html relies on layoutTestController.workerThreadCount
// which is not currently implemented.
TEST_F(WorkerTest, DISABLED_WorkerLifecycle) {
  RunWorkerFastLayoutTest("worker-lifecycle.html");
}

TEST_F(WorkerTest, WorkerLocation) {
  RunWorkerFastLayoutTest("worker-location.html");
}

TEST_F(WorkerTest, WorkerMapGc) {
  RunWorkerFastLayoutTest("wrapper-map-gc.html");
}

TEST_F(WorkerTest, WorkerMessagePort) {
  RunWorkerFastLayoutTest("worker-messageport.html");
}

TEST_F(WorkerTest, WorkerMessagePortGC) {
  RunWorkerFastLayoutTest("worker-messageport-gc.html");
}

TEST_F(WorkerTest, WorkerMultiPort) {
  RunWorkerFastLayoutTest("worker-multi-port.html");
}

TEST_F(WorkerTest, WorkerNavigator) {
  RunWorkerFastLayoutTest("worker-navigator.html");
}

TEST_F(WorkerTest, WorkerReplaceGlobalConstructor) {
  RunWorkerFastLayoutTest("worker-replace-global-constructor.html");
}

TEST_F(WorkerTest, WorkerReplaceSelf) {
  RunWorkerFastLayoutTest("worker-replace-self.html");
}

// http://crbug.com/38918
TEST_F(WorkerTest, DISABLED_WorkerScriptError) {
  RunWorkerFastLayoutTest("worker-script-error.html");
}

// http://crbug.com/36630.
TEST_F(WorkerTest, DISABLED_WorkerTerminate) {
  RunWorkerFastLayoutTest("worker-terminate.html");
}

TEST_F(WorkerTest, WorkerTimeout) {
  RunWorkerFastLayoutTest("worker-timeout.html");
}

//
// SharedWorkerFastLayoutTests
//
// http://crbug.com/27636 - incorrect URL_MISMATCH exceptions sometimes get
// generated on the windows try bots. FLAKY on Win.
// http://crbug.com/28445 - flakiness on mac
// http://crbug.com/36630 - termination issues, disabled on all platforms.
TEST_F(WorkerTest, DISABLED_SharedWorkerFastConstructor) {
  RunWorkerFastLayoutTest("shared-worker-constructor.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastContextGC) {
  RunWorkerFastLayoutTest("shared-worker-context-gc.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastEventListener) {
  RunWorkerFastLayoutTest("shared-worker-event-listener.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastException) {
  RunWorkerFastLayoutTest("shared-worker-exception.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastGC) {
  RunWorkerFastLayoutTest("shared-worker-gc.html");
}

TEST_F(WorkerTest, DISABLED_SharedWorkerFastInIframe) {
  RunWorkerFastLayoutTest("shared-worker-in-iframe.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastLoadError) {
  RunWorkerFastLayoutTest("shared-worker-load-error.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastLocation) {
  RunWorkerFastLayoutTest("shared-worker-location.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastName) {
  RunWorkerFastLayoutTest("shared-worker-name.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastNavigator) {
  RunWorkerFastLayoutTest("shared-worker-navigator.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastReplaceGlobalConstructor) {
  RunWorkerFastLayoutTest("shared-worker-replace-global-constructor.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastReplaceSelf) {
  RunWorkerFastLayoutTest("shared-worker-replace-self.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastScriptError) {
  RunWorkerFastLayoutTest("shared-worker-script-error.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastShared) {
  RunWorkerFastLayoutTest("shared-worker-shared.html");
}

TEST_F(WorkerTest,  DISABLED_SharedWorkerFastSimple) {
  RunWorkerFastLayoutTest("shared-worker-simple.html");
}

// Flaky, http://crbug.com/16934.
TEST_F(WorkerTest, FLAKY_WorkerHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "shared-worker-importScripts.html",
    "shared-worker-redirect.html",
    // flakey? BUG 16934 "text-encoding.html",
#if defined(OS_WIN)
    // Fails on the mac (and linux?):
    // http://code.google.com/p/chromium/issues/detail?id=22599
    "worker-importScripts.html",
#endif
    "worker-redirect.html",
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("LayoutTests");
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, kHttpPort);

  StartHttpServer(new_http_root_dir_);
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  StopHttpServer();
}

// This test has been marked flaky as it fails randomly on the builders.
// http://code.google.com/p/chromium/issues/detail?id=33247
// http://crbug.com/36630 termination issues, disabled on all platforms.
TEST_F(WorkerTest, DISABLED_WorkerWebSocketLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "worker-simple.html",
    "shared-worker-simple.html",
  };

  FilePath websocket_test_dir;
  websocket_test_dir = websocket_test_dir.AppendASCII("LayoutTests");
  websocket_test_dir = websocket_test_dir.AppendASCII("websocket");
  websocket_test_dir = websocket_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(websocket_test_dir, worker_test_dir, kWebSocketPort);
  test_case_dir_ = test_case_dir_.AppendASCII("websocket");
  test_case_dir_ = test_case_dir_.AppendASCII("tests");
  test_case_dir_ = test_case_dir_.AppendASCII("workers");

  ui_test_utils::TestWebSocketServer websocket_server(
      temp_test_dir_.AppendASCII("LayoutTests"));
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kWebSocketPort);
}

TEST_F(WorkerTest, DISABLED_WorkerXhrHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "abort-exception-assert.html",
#if defined(OS_WIN)
    // Fails on the mac (and linux?):
    // http://code.google.com/p/chromium/issues/detail?id=22599
    "close.html",
#endif
    // These tests (and the shared-worker versions below) are disabled due to
    // limitations in lighttpd (doesn't handle all of the HTTP methods).
    //"methods-async.html",
    //"methods.html",

    "shared-worker-close.html",
    // Disabled due to limitations in lighttpd (does not handle methods other
    // than GET/PUT/POST).
    //"shared-worker-methods-async.html",
    //"shared-worker-methods.html",
    "shared-worker-xhr-file-not-found.html",

    "xmlhttprequest-file-not-found.html"
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("LayoutTests");
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("xmlhttprequest");
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, kHttpPort);

  StartHttpServer(new_http_root_dir_);
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  StopHttpServer();
}

// Flaky, http://crbug.com/34996.
TEST_F(WorkerTest, FLAKY_MessagePorts) {
  static const char* kLayoutTestFiles[] = {
    "message-channel-gc.html",
    "message-channel-gc-2.html",
    "message-channel-gc-3.html",
    "message-channel-gc-4.html",
    "message-port.html",
    "message-port-clone.html",
    "message-port-constructor-for-deleted-document.html",
    "message-port-deleted-document.html",
    "message-port-deleted-frame.html",
    "message-port-inactive-document.html",
    "message-port-multi.html",
    "message-port-no-wrapper.html",
    // Only works with run-webkit-tests --leaks.
    //"message-channel-listener-circular-ownership.html",
  };

  FilePath fast_test_dir;
  fast_test_dir = fast_test_dir.AppendASCII("LayoutTests");
  fast_test_dir = fast_test_dir.AppendASCII("fast");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("events");
  InitializeForLayoutTest(fast_test_dir, worker_test_dir, kNoHttpPort);

  // MessagePort tests also rely on common files in js/resources.
  FilePath js_dir = fast_test_dir.AppendASCII("js");
  FilePath resource_dir;
  resource_dir = resource_dir.AppendASCII("resources");
  AddResourceForLayoutTest(js_dir, resource_dir);

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kNoHttpPort);
}

// http://crbug.com/30307, disabled on Linux
// This has been flaky on Windows since r39931. http://crbug.com/36800
// http://crbug.com/36630. Termination issues, disabled on all platforms.
TEST_F(WorkerTest, DISABLED_LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  NavigateToURL(url);
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
}

// Doesn't crash, but on all platforms, it sometimes fails.
// http://crbug.com/28445. Made FLAKY
// http://crbug.com/36630. Termination issues, disabled on all platforms.
TEST_F(WorkerTest, DISABLED_LimitTotal) {
#if !defined(OS_LINUX)
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  int total_workers = WorkerService::kMaxWorkersWhenSeparate;

  int tab_count = (total_workers / max_workers_per_tab) + 1;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  for (int i = 1; i < tab_count; ++i)
    ASSERT_TRUE(window->AppendTab(url));

  // Check that we didn't create more than the max number of workers.
  ASSERT_TRUE(WaitForProcessCountToBe(tab_count, total_workers));

  // Now close a page and check that the queued workers were started.
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab->NavigateToURL(GetTestUrl(L"google", L"google.html")));

  ASSERT_TRUE(WaitForProcessCountToBe(tab_count, total_workers));
#endif
}

// http://crbug.com/36630 termination issues, disabled on all paltforms.
TEST_F(WorkerTest, DISABLED_WorkerClose) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  GURL url = GetTestUrl(L"workers", L"worker_close.html");
  ASSERT_TRUE(tab->NavigateToURL(url));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(1, 0));
}

TEST_F(WorkerTest, QueuedSharedWorkerShutdown) {
  // Tests to make sure that queued shared workers are started up when
  // shared workers shut down.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"queued_shared_worker_shutdown.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
}

TEST_F(WorkerTest, MultipleTabsQueuedSharedWorker) {
  // Tests to make sure that only one instance of queued shared workers are
  // started up even when those instances are on multiple tabs.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_shared_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab+1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));

  // Create same set of workers in new tab (leaves one worker queued from this
  // tab).
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  ASSERT_TRUE(window->AppendTab(url));
  ASSERT_TRUE(WaitForProcessCountToBe(2, max_workers_per_tab));

  // Now shutdown one of the shared workers - this will fire both queued
  // workers, but only one instance should be started
  GURL url2 = GetTestUrl(L"workers", L"shutdown_shared_worker.html?id=0");
  ASSERT_TRUE(window->AppendTab(url2));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(3, max_workers_per_tab));
}

// http://crbug.com/36630 termination issues, disabled on all paltforms.
TEST_F(WorkerTest, DISABLED_QueuedSharedWorkerStartedFromOtherTab) {
  // Tests to make sure that queued shared workers are started up when
  // an instance is launched from another tab.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_shared_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab+1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
  // First window has hit its limit. Now launch second window which creates
  // the same worker that was queued in the first window, to ensure it gets
  // connected to the first window too.
  scoped_refptr<BrowserProxy> window(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(window.get());
  GURL url2 = GetTestUrl(L"workers", L"single_shared_worker.html");
  url2 = GURL(url2.spec() + StringPrintf("?id=%d", max_workers_per_tab));
  ASSERT_TRUE(window->AppendTab(url2));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(2, max_workers_per_tab+1));
}
