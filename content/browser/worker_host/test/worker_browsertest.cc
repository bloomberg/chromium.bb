// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/test/test_timeouts.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "net/base/test_data_directory.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "url/gurl.h"

namespace content {

class WorkerTest : public ContentBrowserTest {
 public:
  WorkerTest() {}

  GURL GetTestURL(const std::string& test_case, const std::string& query) {
    base::FilePath test_file_path = GetTestFilePath(
        "workers", test_case.c_str());
    return GetFileUrlWithQuery(test_file_path, query);
  }

  void RunTest(Shell* window,
               const std::string& test_case,
               const std::string& query) {
    GURL url = GetTestURL(test_case, query);
    const base::string16 expected_title = base::ASCIIToUTF16("OK");
    TitleWatcher title_watcher(window->web_contents(), expected_title);
    NavigateToURL(window, url);
    base::string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, final_title);
  }

  void RunTest(const std::string& test_case, const std::string& query) {
    RunTest(shell(), test_case, query);
  }

  static void CountWorkerProcesses(int *cur_process_count) {
    *cur_process_count = 0;
    for (WorkerProcessHostIterator iter; !iter.Done(); ++iter)
      (*cur_process_count)++;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::MessageLoop::QuitClosure());
  }

  bool WaitForWorkerProcessCount(int count) {
    int cur_process_count;
    for (int i = 0; i < 100; ++i) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&CountWorkerProcesses, &cur_process_count));

      RunMessageLoop();
      if (cur_process_count == count)
        return true;

      // Sometimes the worker processes can take a while to shut down on the
      // bots, so use a longer timeout period to avoid spurious failures.
      base::PlatformThread::Sleep(TestTimeouts::action_max_timeout() / 100);
    }

    EXPECT_EQ(cur_process_count, count);
    return false;
  }

  static void QuitUIMessageLoop(base::Callback<void()> callback) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
  }

  void NavigateAndWaitForAuth(const GURL& url) {
    ShellContentBrowserClient* browser_client =
        ShellContentBrowserClient::Get();
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    browser_client->resource_dispatcher_host_delegate()->
        set_login_request_callback(
            base::Bind(&QuitUIMessageLoop, runner->QuitClosure()));
    shell()->LoadURL(url);
    runner->Run();
  }
};

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorker) {
  RunTest("single_worker.html", std::string());
}

IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleWorkers) {
  RunTest("multi_worker.html", std::string());
}

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleSharedWorker) {
  RunTest("single_worker.html", "shared=true");
}

// http://crbug.com/96435
IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleSharedWorkers) {
  RunTest("multi_worker.html", "shared=true");
}

// Incognito windows should not share workers with non-incognito windows
// http://crbug.com/30021
IN_PROC_BROWSER_TEST_F(WorkerTest, IncognitoSharedWorkers) {
  // Load a non-incognito tab and have it create a shared worker
  RunTest("incognito_worker.html", std::string());

  // Incognito worker should not share with non-incognito
  RunTest(CreateOffTheRecordBrowser(), "incognito_worker.html", std::string());
}

// Make sure that auth dialog is displayed from worker context.
// http://crbug.com/33344
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerHttpAuth) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/workers/worker_auth.html");

  NavigateAndWaitForAuth(url);
}

// Make sure that auth dialog is displayed from shared worker context.
// http://crbug.com/33344
IN_PROC_BROWSER_TEST_F(WorkerTest, SharedWorkerHttpAuth) {
  ASSERT_TRUE(test_server()->Start());
  GURL url = test_server()->GetURL("files/workers/shared_worker_auth.html");
  NavigateAndWaitForAuth(url);
}

#if defined(OS_LINUX)
// This test is flaky inside the Linux SUID sandbox.
// http://crbug.com/130116
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_LimitPerPage) {
#else
IN_PROC_BROWSER_TEST_F(WorkerTest, LimitPerPage) {
#endif
  // There is no limitation of SharedWorker if EmbeddedSharedWorker is enabled.
  if (WorkerService::EmbeddedSharedWorkerEnabled())
    return;
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate;
  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab + 1);

  GURL url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(shell(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));
}


#if defined(OS_LINUX) || defined(OS_MACOSX)
// This test is flaky inside the Linux SUID sandbox: http://crbug.com/130116
// Also flaky on Mac: http://crbug.com/295193
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_LimitTotal) {
#else
// http://crbug.com/36800
IN_PROC_BROWSER_TEST_F(WorkerTest, LimitTotal) {
#endif
  // There is no limitation of SharedWorker if EmbeddedSharedWorker is enabled.
  if (WorkerService::EmbeddedSharedWorkerEnabled())
    return;
  if (base::SysInfo::AmountOfPhysicalMemoryMB() < 8192) {
    VLOG(0) << "WorkerTest.LimitTotal not running because it needs 8 GB RAM.";
    return;
  }

  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate;
  int total_workers = WorkerServiceImpl::kMaxWorkersWhenSeparate;

  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab);
  GURL url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(shell(),
                GURL(url.spec() + base::StringPrintf("&client_id=0")));

  // Adding 1 so that we cause some workers to be queued.
  int tab_count = (total_workers / max_workers_per_tab) + 1;
  for (int i = 1; i < tab_count; ++i) {
    NavigateToURL(
        CreateBrowser(),
        GURL(url.spec() + base::StringPrintf("&client_id=%d", i)));
  }

  // Check that we didn't create more than the max number of workers.
  ASSERT_TRUE(WaitForWorkerProcessCount(total_workers));

  // Now close a page and check that the queued workers were started.
  url = GURL(GetTestUrl("google", "google.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(WaitForWorkerProcessCount(total_workers));
}

// Flaky, http://crbug.com/59786.
IN_PROC_BROWSER_TEST_F(WorkerTest, WorkerClose) {
  RunTest("worker_close.html", std::string());
  ASSERT_TRUE(WaitForWorkerProcessCount(0));
}

// Flaky, http://crbug.com/70861.
// Times out regularly on Windows debug bots. See http://crbug.com/212339 .
// Times out on Mac as well.
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_QueuedSharedWorkerShutdown) {
  // Tests to make sure that queued shared workers are started up when shared
  // workers shut down.
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate;
  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab);
  RunTest("queued_shared_worker_shutdown.html", query);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));
}

// Flaky, http://crbug.com/69881.
// Sometimes triggers
//     Check failed: message_ports_[message_port_id].queued_messages.empty().
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_MultipleTabsQueuedSharedWorker) {
  // Tests to make sure that only one instance of queued shared workers are
  // started up even when those instances are on multiple tabs.
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate;
  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab + 1);
  GURL url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(shell(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));

  // Create same set of workers in new tab (leaves one worker queued from this
  // tab).
  url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(CreateBrowser(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));

  // Now shutdown one of the shared workers - this will fire both queued
  // workers, but only one instance should be started.
  url = GetTestURL("shutdown_shared_worker.html", "?id=0");
  NavigateToURL(CreateBrowser(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));
}

// Flaky: http://crbug.com/48148
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_QueuedSharedWorkerStartedFromOtherTab) {
  // Tests to make sure that queued shared workers are started up when
  // an instance is launched from another tab.
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerFrameWhenSeparate;
  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab + 1);
  GURL url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(shell(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));

  // First window has hit its limit. Now launch second window which creates
  // the same worker that was queued in the first window, to ensure it gets
  // connected to the first window too.
  query = base::StringPrintf("?id=%d", max_workers_per_tab);
  url = GetTestURL("single_shared_worker.html", query);
  NavigateToURL(CreateBrowser(), url);

  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab + 1));
}

IN_PROC_BROWSER_TEST_F(WorkerTest, WebSocketSharedWorker) {
  // Launch WebSocket server.
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::SpawnedTestServer::kLocalhost,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  // Generate test URL.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  GURL url = ws_server.GetURL(
      "websocket_shared_worker.html").ReplaceComponents(replacements);

  // Run test.
  Shell* window = shell();
  const base::string16 expected_title = base::ASCIIToUTF16("OK");
  TitleWatcher title_watcher(window->web_contents(), expected_title);
  NavigateToURL(window, url);
  base::string16 final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

IN_PROC_BROWSER_TEST_F(WorkerTest, PassMessagePortToSharedWorker) {
  RunTest("pass_messageport_to_sharedworker.html", "");
}

IN_PROC_BROWSER_TEST_F(WorkerTest,
                       PassMessagePortToSharedWorkerDontWaitForConnect) {
  RunTest("pass_messageport_to_sharedworker_dont_wait_for_connect.html", "");
}

}  // namespace content
