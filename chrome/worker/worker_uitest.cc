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
#include "net/url_request/url_request_unittest.h"

static const char kTestCompleteCookie[] = "status";
static const char kTestCompleteSuccess[] = "OK";

// Layout test files for WorkerFastLayoutTest for the WorkerFastLayoutTest
// shards.
static const char* kWorkerFastLayoutTestFiles[] = {
  "stress-js-execution.html",
  "use-machine-stack.html",
  "worker-call.html",
#if defined(OS_WIN)
  // This test occasionally fails on valgrind (http://crbug.com/30212).
  "worker-cloneport.html",
#endif
  "worker-close.html",
  "worker-constructor.html",
  "worker-context-gc.html",
  "worker-context-multi-port.html",
  "worker-event-listener.html",
  "worker-gc.html",
  // worker-lifecycle.html relies on layoutTestController.workerThreadCount
  // which is not currently implemented.
  // "worker-lifecycle.html",
  "worker-location.html",
  "worker-messageport.html",
  // Disabled after r27089 (WebKit merge), http://crbug.com/22947
  // "worker-messageport-gc.html",
  "worker-multi-port.html",
  "worker-navigator.html",
  "worker-replace-global-constructor.html",
  "worker-replace-self.html",
  "worker-script-error.html",
  "worker-terminate.html",
  "worker-timeout.html"
};
static const int kWorkerFastLayoutTestShards = 3;

class WorkerTest : public UILayoutTest {
 protected:
  virtual ~WorkerTest() { }

  void RunTest(const std::wstring& test_case) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    GURL url = GetTestUrl(L"workers", test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, kTestIntervalMs, kTestWaitTimeoutMs);
    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }

  void RunIncognitoTest(const std::wstring& test_case) {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    // Open an Incognito window.
    int window_count;
    ASSERT_TRUE(browser->RunCommand(IDC_NEW_INCOGNITO_WINDOW));
    scoped_refptr<BrowserProxy> incognito(automation()->GetBrowserWindow(1));
    scoped_refptr<TabProxy> tab(incognito->GetTab(0));
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(2, window_count);

    GURL url = GetTestUrl(L"workers", test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, kTestIntervalMs, kTestWaitTimeoutMs);

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

  void RunWorkerFastLayoutTests(size_t shard) {
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

    for (size_t i = 0; i < arraysize(kWorkerFastLayoutTestFiles); ++i) {
      if ((i % kWorkerFastLayoutTestShards) == shard) {
        printf ("Test: %s\n", kWorkerFastLayoutTestFiles[i]);
        RunLayoutTest(kWorkerFastLayoutTestFiles[i], kNoHttpPort);
      }
    }

    // Navigate away from to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    GURL about_url(std::string("file://localhost/"));
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(about_url));
  }

  bool NavigateAndWaitForAuth(TabProxy* tab, const GURL& url) {
    // Pass a large number of navigations to tell the tab to block until an auth
    // dialog pops up.
    bool timeout = false;
    tab->NavigateToURLWithTimeout(url, 100, kTestWaitTimeoutMs, &timeout);
    EXPECT_FALSE(timeout);
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

// Make sure that auth dialog is displayed from worker context.
TEST_F(WorkerTest, WorkerHttpAuth) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);

  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<TabProxy> tab(GetActiveTab());
  GURL url = server->TestServerPage("files/worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
}

// Make sure that auth dialog is displayed from shared worker context.
TEST_F(WorkerTest, SharedWorkerHttpAuth) {
  scoped_refptr<HTTPTestServer> server =
      HTTPTestServer::CreateServer(kDocRoot, NULL);
  ASSERT_TRUE(NULL != server.get());
  scoped_refptr<TabProxy> tab(GetActiveTab());
  EXPECT_EQ(1, GetTabCount());
  GURL url = server->TestServerPage("files/shared_worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
  // TODO(atwilson): Add support to automation framework to test for auth
  // dialogs displayed by non-navigating tabs.
}

#if defined(OS_LINUX)
// Crashes on Linux - http://crbug.com/22898
#define WorkerFastLayoutTests0 DISABLED_WorkerFastLayoutTests0
#define WorkerFastLayoutTests1 DISABLED_WorkerFastLayoutTests1
#define WorkerFastLayoutTests2 DISABLED_WorkerFastLayoutTests2
#elif defined(OS_MACOSX)
// Flaky on Mac - http://crbug.com/28445
#define WorkerFastLayoutTests0 FLAKY_WorkerFastLayoutTests0
#endif

TEST_F(WorkerTest, WorkerFastLayoutTests0) {
  SCOPED_TRACE("");
  RunWorkerFastLayoutTests(0);
}

TEST_F(WorkerTest, WorkerFastLayoutTests1) {
  SCOPED_TRACE("");
  RunWorkerFastLayoutTests(1);
}

TEST_F(WorkerTest, WorkerFastLayoutTests2) {
  SCOPED_TRACE("");
  RunWorkerFastLayoutTests(2);
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// http://crbug.com/27636 - incorrect URL_MISMATCH exceptions sometimes get
// generated on the windows try bots.
// http://crbug.com/28445 - flakiness on mac
#define SharedWorkerFastLayoutTests FLAKY_SharedWorkerFastLayoutTests
#endif

TEST_F(WorkerTest, SharedWorkerFastLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "shared-worker-constructor.html",
    "shared-worker-context-gc.html",
    "shared-worker-event-listener.html",
    "shared-worker-exception.html",
    "shared-worker-gc.html",
    // Lifecycle tests rely on layoutTestController.workerThreadCount which is
    // not currently implemented.
    //"shared-worker-frame-lifecycle.html",
    //"shared-worker-lifecycle.html",
    "shared-worker-load-error.html",
    "shared-worker-location.html",
    "shared-worker-name.html",
    "shared-worker-navigator.html",
    "shared-worker-replace-global-constructor.html",
    "shared-worker-replace-self.html",
    "shared-worker-script-error.html",
    "shared-worker-shared.html",
    "shared-worker-simple.html",
  };

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

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i) {
    RunLayoutTest(kLayoutTestFiles[i], kNoHttpPort);
    // Shared workers will error out if we ever have more than one tab open.
    int window_count = 0;
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);
    EXPECT_EQ(1, GetTabCount());
  }
}

TEST_F(WorkerTest, WorkerHttpLayoutTests) {
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
TEST_F(WorkerTest, FLAKY_WorkerWebSocketLayoutTests) {
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

  StartWebSocketServer(temp_test_dir_.AppendASCII("LayoutTests"));
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kWebSocketPort);
  StopWebSocketServer();
}

TEST_F(WorkerTest, WorkerXhrHttpLayoutTests) {
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

TEST_F(WorkerTest, MessagePorts) {
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

#if defined(OS_LINUX)
// http://crbug.com/30307
#define LimitPerPage DISABLED_LimitPerPage
#endif

TEST_F(WorkerTest, LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));

  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
}

// Doesn't crash, but on all platforms, it sometimes fails.
// http://crbug.com/28445
#define LimitTotal FLAKY_LimitTotal

TEST_F(WorkerTest, LimitTotal) {
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
  for (int i = 1; i < tab_count; ++i)
    window->AppendTab(url);

  // Check that we didn't create more than the max number of workers.
  ASSERT_TRUE(WaitForProcessCountToBe(tab_count, total_workers));

  // Now close a page and check that the queued workers were started.
  tab->NavigateToURL(GetTestUrl(L"google", L"google.html"));

  ASSERT_TRUE(WaitForProcessCountToBe(tab_count, total_workers));
#endif
}
