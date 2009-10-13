// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/worker_host/worker_service.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"

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
        kTestCompleteCookie, kTestIntervalMs, kTestWaitTimeoutMs);
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
};

TEST_F(WorkerTest, SingleWorker) {
  RunTest(L"single_worker.html");
}

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(L"multi_worker.html");
}

// WorkerFastLayoutTests works on the linux try servers.
#if defined(OS_LINUX)
#define WorkerFastLayoutTests DISABLED_WorkerFastLayoutTests
#endif

TEST_F(WorkerTest, WorkerFastLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "stress-js-execution.html",
#if defined(OS_WIN)
    // Workers don't properly initialize the V8 stack guard.
    // (http://code.google.com/p/chromium/issues/detail?id=21653).
    "use-machine-stack.html",
#endif
    "worker-call.html",
    // Disabled because cloning ports are too slow in Chromium to meet the
    // thresholds in this test.
    // http://code.google.com/p/chromium/issues/detail?id=22780
    // "worker-cloneport.html",

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
    // Disabled afer r27553 (WebKit merge), http://crbug.com/23391
    // "worker-script-error.html",
    "worker-terminate.html",
    "worker-timeout.html"
  };

  FilePath fast_test_dir;
  fast_test_dir = fast_test_dir.AppendASCII("LayoutTests");
  fast_test_dir = fast_test_dir.AppendASCII("fast");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(fast_test_dir, worker_test_dir, false);

  // Worker tests also rely on common files in js/resources.
  FilePath js_dir = fast_test_dir.AppendASCII("js");
  FilePath resource_dir;
  resource_dir = resource_dir.AppendASCII("resources");
  AddResourceForLayoutTest(js_dir, resource_dir);

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], false);
}

TEST_F(WorkerTest, WorkerHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
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
  InitializeForLayoutTest(http_test_dir, worker_test_dir, true);

  StartHttpServer(new_http_root_dir_);
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], true);
  StopHttpServer();
}

TEST_F(WorkerTest, WorkerXhrHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "abort-exception-assert.html",
#if defined(OS_WIN)
    // Fails on the mac (and linux?):
    // http://code.google.com/p/chromium/issues/detail?id=22599
    "close.html",
#endif
    //"methods-async.html",
    //"methods.html",
    "xmlhttprequest-file-not-found.html"
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("LayoutTests");
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("xmlhttprequest");
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, true);

  StartHttpServer(new_http_root_dir_);
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], true);
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
    // http://code.google.com/p/chromium/issues/detail?id=23709
    // "message-port-constructor-for-deleted-document.html",
    "message-port-deleted-document.html",
    "message-port-deleted-frame.html",
    // http://crbug.com/23597 (caused by http://trac.webkit.org/changeset/48978)
    // "message-port-inactive-document.html",
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
  InitializeForLayoutTest(fast_test_dir, worker_test_dir, false);

  // MessagePort tests also rely on common files in js/resources.
  FilePath js_dir = fast_test_dir.AppendASCII("js");
  FilePath resource_dir;
  resource_dir = resource_dir.AppendASCII("resources");
  AddResourceForLayoutTest(js_dir, resource_dir);

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], false);
}

// Disable LimitPerPage on Linux. Seems to work on Mac though:
// http://code.google.com/p/chromium/issues/detail?id=22608
#if !defined(OS_LINUX)
// This test fails after WebKit merge 49414:49432. (BUG=24652)
TEST_F(WorkerTest, LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = GetTestUrl(L"workers", L"many_workers.html");
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));

  EXPECT_EQ(max_workers_per_tab + 1 + (UITest::in_process_renderer() ? 0 : 1),
            UITest::GetBrowserProcessCount());
}
#endif

TEST_F(WorkerTest, LimitTotal) {
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
}
