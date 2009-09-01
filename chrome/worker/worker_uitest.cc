// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
};

TEST_F(WorkerTest, SingleWorker) {
  RunTest(L"single_worker.html");
}

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(L"multi_worker.html");
}

TEST_F(WorkerTest, DISABLED_WorkerFastLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "stress-js-execution.html",
    "use-machine-stack.html",
    "worker-call.html",
    //"worker-close.html",
    "worker-constructor.html",
    "worker-context-gc.html",
    "worker-event-listener.html",
    "worker-gc.html",
    "worker-location.html",
    "worker-messageport.html",
    "worker-navigator.html",
    "worker-replace-global-constructor.html",
    "worker-replace-self.html",
    "worker-script-error.html",
    "worker-terminate.html",
    "worker-timeout.html"
  };

  FilePath fast_test_dir;
  fast_test_dir = fast_test_dir.AppendASCII("LayoutTests");
  fast_test_dir = fast_test_dir.AppendASCII("fast");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(fast_test_dir, worker_test_dir, false);

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], false);
}

TEST_F(WorkerTest, WorkerHttpLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    // flakey? BUG 16934 "text-encoding.html",
    "worker-importScripts.html",
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
    "close.html",
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
    "message-port-constructor-for-deleted-document.html",
    "message-port-deleted-document.html",
    "message-port-deleted-frame.html",
    "message-port-inactive-document.html",
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

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], false);
}

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
  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());

  // Now close the first tab and check that the queued workers were started.
  tab->Close(true);
  tab->NavigateToURL(GetTestUrl(L"google", L"google.html"));

  EXPECT_EQ(total_workers + 1 + (UITest::in_process_renderer() ? 0 : tab_count),
            UITest::GetBrowserProcessCount());
}
