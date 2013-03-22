// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/browser/worker_host/worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_resource_dispatcher_host_delegate.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/layout_browsertest.h"
#include "googleurl/src/gurl.h"
#include "net/base/test_data_directory.h"
#include "net/test/test_server.h"

namespace content {

class WorkerLayoutTest : public InProcessBrowserLayoutTest {
 public:
  WorkerLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath(),
      base::FilePath().AppendASCII("fast").AppendASCII("workers")) {
  }
};

// Crashy, http://crbug.com/35965.
// Flaky, http://crbug.com/36555.
IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, DISABLED_WorkerClonePort) {
  RunLayoutTest("worker-cloneport.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, WorkerContextMultiPort) {
  RunLayoutTest("worker-context-multi-port.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, WorkerMessagePort) {
  RunLayoutTest("worker-messageport.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, WorkerMessagePortGC) {
  RunLayoutTest("worker-messageport-gc.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, WorkerMultiPort) {
  RunLayoutTest("worker-multi-port.html");
}

//
// SharedWorkerFastLayoutTests
//
IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastConstructor) {
  RunLayoutTest("shared-worker-constructor.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastContextGC) {
  RunLayoutTest("shared-worker-context-gc.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastEventListener) {
  RunLayoutTest("shared-worker-event-listener.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastException) {
  RunLayoutTest("shared-worker-exception.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastGC) {
  RunLayoutTest("shared-worker-gc.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastInIframe) {
  RunLayoutTest("shared-worker-in-iframe.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastLoadError) {
  RunLayoutTest("shared-worker-load-error.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastLocation) {
  RunLayoutTest("shared-worker-location.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastName) {
  RunLayoutTest("shared-worker-name.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastNavigator) {
  RunLayoutTest("shared-worker-navigator.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest,
                       SharedWorkerFastReplaceGlobalConstructor) {
  RunLayoutTest("shared-worker-replace-global-constructor.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastReplaceSelf) {
  RunLayoutTest("shared-worker-replace-self.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastScriptError) {
  RunLayoutTest("shared-worker-script-error.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastShared) {
  RunLayoutTest("shared-worker-shared.html");
}

IN_PROC_BROWSER_TEST_F(WorkerLayoutTest, SharedWorkerFastSimple) {
  RunLayoutTest("shared-worker-simple.html");
}

class MessagePortTest : public InProcessBrowserLayoutTest {
 public:
  MessagePortTest() : InProcessBrowserLayoutTest(
      base::FilePath(),
      base::FilePath().AppendASCII("fast").AppendASCII("events")) {
  }
};

// Flaky, http://crbug.com/34996.
IN_PROC_BROWSER_TEST_F(MessagePortTest, Tests) {
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
    // "message-channel-listener-circular-ownership.html",
  };

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i]);
}


class WorkerHttpLayoutTest : public InProcessBrowserLayoutTest {
 public:
  // The resources for these tests hardcode 8000, so must use that here. If
  // multiple tests which use it run in parallel, then the test will fail but
  // it'll run again at the end in serial and pass.
  WorkerHttpLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath().AppendASCII("http").AppendASCII("tests"),
      base::FilePath().AppendASCII("workers"),
      8000) {
  }
};

// http://crbug.com/16934
IN_PROC_BROWSER_TEST_F(WorkerHttpLayoutTest, DISABLED_Tests) {
  static const char* kLayoutTestFiles[] = {
      "shared-worker-importScripts.html",
      "shared-worker-redirect.html",
      "text-encoding.html",
#if defined(OS_WIN)
      // Fails on the mac (and linux?):
      // http://code.google.com/p/chromium/issues/detail?id=22599
      "worker-importScripts.html",
#endif
      "worker-redirect.html",
  };

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunHttpLayoutTest(kLayoutTestFiles[i]);
}

class WorkerXHRHttpLayoutTest : public InProcessBrowserLayoutTest {
 public:
  WorkerXHRHttpLayoutTest() : InProcessBrowserLayoutTest(
      base::FilePath().AppendASCII("http").AppendASCII("tests"),
      base::FilePath().AppendASCII("xmlhttprequest").AppendASCII("workers"),
      -1) {
  }
};

// TestRunner appears to be broken on Windows. See http://crbug.com/177798
#if defined(OS_WIN)
#define MAYBE_Tests DISABLED_Tests
#else
#define MAYBE_Tests Tests
#endif
IN_PROC_BROWSER_TEST_F(WorkerXHRHttpLayoutTest, MAYBE_Tests) {
  static const char* kLayoutTestFiles[] = {
    // worker thread count never drops to zero.
    // http://crbug.com/150565
    // "abort-exception-assert.html",
#if defined(OS_WIN)
    // Fails on the mac (and linux?):
    // http://code.google.com/p/chromium/issues/detail?id=22599
    "close.html",
#endif
    // These tests (and the shared-worker versions below) are disabled due to
    // limitations in lighttpd (doesn't handle all of the HTTP methods).
    // "methods-async.html",
    // "methods.html",

    "shared-worker-close.html",
    // Disabled due to limitations in lighttpd (does not handle methods other
    // than GET/PUT/POST).
    // "shared-worker-methods-async.html",
    // "shared-worker-methods.html",
    "shared-worker-xhr-file-not-found.html",

    "xmlhttprequest-file-not-found.html"
  };

  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunHttpLayoutTest(kLayoutTestFiles[i]);
}

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
    const string16 expected_title = ASCIIToUTF16("OK");
    TitleWatcher title_watcher(window->web_contents(), expected_title);
    NavigateToURL(window, url);
    string16 final_title = title_watcher.WaitAndGetTitle();
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
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
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
        static_cast<ShellContentBrowserClient*>(GetContentClient()->browser());
    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    browser_client->resource_dispatcher_host_delegate()->
        set_login_request_callback(
            base::Bind(&QuitUIMessageLoop, runner->QuitClosure()));
    shell()->LoadURL(url);
    runner->Run();
  }
};

IN_PROC_BROWSER_TEST_F(WorkerTest, SingleWorker) {
  RunTest("single_worker.html", "");
}

IN_PROC_BROWSER_TEST_F(WorkerTest, MultipleWorkers) {
  RunTest("multi_worker.html", "");
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
  RunTest("incognito_worker.html", "");

  // Incognito worker should not share with non-incognito
  RunTest(CreateOffTheRecordBrowser(), "incognito_worker.html", "");
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// This test is flaky inside the Linux SUID sandbox.
// http://crbug.com/130116
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_LimitPerPage) {
#else
IN_PROC_BROWSER_TEST_F(WorkerTest, LimitPerPage) {
#endif
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate;
  std::string query = base::StringPrintf("?count=%d", max_workers_per_tab + 1);

  GURL url = GetTestURL("many_shared_workers.html", query);
  NavigateToURL(shell(), url);
  ASSERT_TRUE(WaitForWorkerProcessCount(max_workers_per_tab));
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// This test is flaky inside the Linux SUID sandbox.
// http://crbug.com/130116
IN_PROC_BROWSER_TEST_F(WorkerTest, DISABLED_LimitTotal) {
#else
// http://crbug.com/36800
IN_PROC_BROWSER_TEST_F(WorkerTest, LimitTotal) {
#endif
  if (base::SysInfo::AmountOfPhysicalMemoryMB() < 8192) {
    LOG(INFO) << "WorkerTest.LimitTotal not running because it needs 8 GB RAM.";
    return;
  }

  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate;
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
  RunTest("worker_close.html", "");
  ASSERT_TRUE(WaitForWorkerProcessCount(0));
}

// Flaky, http://crbug.com/70861.
// Times out regularly on Windows debug bots. See http://crbug.com/212339 .
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_QueuedSharedWorkerShutdown DISABLED_QueuedSharedWorkerShutdown
#else
#define MAYBE_QueuedSharedWorkerShutdown QueuedSharedWorkerShutdown
#endif
IN_PROC_BROWSER_TEST_F(WorkerTest, MAYBE_QueuedSharedWorkerShutdown) {
  // Tests to make sure that queued shared workers are started up when shared
  // workers shut down.
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate;
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
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate;
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
  int max_workers_per_tab = WorkerServiceImpl::kMaxWorkersPerTabWhenSeparate;
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
  net::TestServer ws_server(net::TestServer::TYPE_WS,
                            net::TestServer::kLocalhost,
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
  const string16 expected_title = ASCIIToUTF16("OK");
  TitleWatcher title_watcher(window->web_contents(), expected_title);
  NavigateToURL(window, url);
  string16 final_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, final_title);
}

}  // namespace content
