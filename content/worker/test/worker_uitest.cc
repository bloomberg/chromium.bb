// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/string_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/layout_test_http_server.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "content/browser/worker_host/worker_service.h"
#include "content/public/common/url_constants.h"
#include "net/test/test_server.h"

namespace {

const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
const FilePath::CharType* kTestDir =
    FILE_PATH_LITERAL("workers");
const FilePath::CharType* kManySharedWorkersFile =
    FILE_PATH_LITERAL("many_shared_workers.html");
const FilePath::CharType* kManyWorkersFile =
    FILE_PATH_LITERAL("many_workers.html");
const FilePath::CharType* kQuerySharedWorkerShutdownFile =
    FILE_PATH_LITERAL("queued_shared_worker_shutdown.html");
const FilePath::CharType* kShutdownSharedWorkerFile =
    FILE_PATH_LITERAL("shutdown_shared_worker.html");
const FilePath::CharType* kSingleSharedWorkersFile =
    FILE_PATH_LITERAL("single_shared_worker.html");
const FilePath::CharType* kWorkerClose =
    FILE_PATH_LITERAL("worker_close.html");

}  // anonymous namespace

class WorkerTest : public UILayoutTest {
 protected:
  virtual ~WorkerTest() { }

  void RunTest(const FilePath& test_case, const std::string& query) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    FilePath test_file_path = ui_test_utils::GetTestFilePath(
        FilePath(kTestDir), test_case);
    GURL url = ui_test_utils::GetFileUrlWithQuery(test_file_path, query);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());
    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }

  void RunIncognitoTest(const FilePath& test_case) {
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

    GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir), test_case);
    ASSERT_TRUE(tab->NavigateToURL(url));

    std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
        kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());

    // Close the incognito window
    ASSERT_TRUE(incognito->RunCommand(IDC_CLOSE_WINDOW));
    ASSERT_TRUE(automation()->GetBrowserWindowCount(&window_count));
    ASSERT_EQ(1, window_count);

    ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  }

  bool WaitForProcessCountToBe(int tabs, int workers) {
    // The 1 is for the browser process.
    int number_of_processes = 1 + workers + tabs;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
    // On Unix, we also have a zygote process and a sandbox host process.
    number_of_processes += 2;
#endif

    int cur_process_count;
    for (int i = 0; i < 100; ++i) {
      cur_process_count = 0;
      if (!GetBrowserProcessCount(&cur_process_count))
        return false;
      if (cur_process_count == number_of_processes)
        return true;

      // Sometimes the worker processes can take a while to shut down on the
      // bots, so use a longer timeout period to avoid spurious failures.
      base::PlatformThread::Sleep(TestTimeouts::action_max_timeout_ms() / 100);
    }

    EXPECT_EQ(number_of_processes, cur_process_count);
    return false;
  }

  void RunWorkerFastLayoutTest(const std::string& test_case_file_name) {
    FilePath fast_test_dir;
    fast_test_dir = fast_test_dir.AppendASCII("fast");

    FilePath worker_test_dir;
    worker_test_dir = worker_test_dir.AppendASCII("workers");
    InitializeForLayoutTest(fast_test_dir, worker_test_dir, kNoHttpPort);

    // Worker tests also rely on common files in js/resources.
    FilePath js_dir = fast_test_dir.AppendASCII("js");
    FilePath resource_dir;
    resource_dir = resource_dir.AppendASCII("resources");
    AddResourceForLayoutTest(js_dir, resource_dir);

    printf("Test: %s\n", test_case_file_name.c_str());
    RunLayoutTest(test_case_file_name, kNoHttpPort);

    // Navigate to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    GURL about_url(chrome::kAboutBlankURL);
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(about_url));
  }

  void RunWorkerStorageLayoutTest(const std::string& test_case_file_name) {
    FilePath worker_test_dir;
    worker_test_dir = worker_test_dir.AppendASCII("fast");
    worker_test_dir = worker_test_dir.AppendASCII("workers");

    FilePath storage_test_dir;
    storage_test_dir = storage_test_dir.AppendASCII("storage");
    InitializeForLayoutTest(worker_test_dir, storage_test_dir, kNoHttpPort);

    // Storage worker tests also rely on common files in 'resources'.
    FilePath resource_dir;
    resource_dir = resource_dir.AppendASCII("resources");
    AddResourceForLayoutTest(worker_test_dir.Append(storage_test_dir),
                             resource_dir);

    printf("Test: %s\n", test_case_file_name.c_str());
    RunLayoutTest(test_case_file_name, kNoHttpPort);

    // Navigate to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    GURL about_url(chrome::kAboutBlankURL);
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
  RunTest(FilePath(FILE_PATH_LITERAL("single_worker.html")), "");
}

TEST_F(WorkerTest, MultipleWorkers) {
  RunTest(FilePath(FILE_PATH_LITERAL("multi_worker.html")), "");
}

TEST_F(WorkerTest, SingleSharedWorker) {
  RunTest(FilePath(FILE_PATH_LITERAL("single_worker.html")), "shared=true");
}

// Flaky on Win XP only.  http://crbug.com/96435
#if defined(OS_WIN)
#define MultipleSharedWorkers FLAKY_MultipleSharedWorkers
#endif
TEST_F(WorkerTest, MultipleSharedWorkers) {
  RunTest(FilePath(FILE_PATH_LITERAL("multi_worker.html")), "shared=true");
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// http://crbug.com/80446
#define DISABLED_TerminateQueuedWorkers FLAKY_TerminateQueuedWorkers
#endif
TEST_F(WorkerTest, DISABLED_TerminateQueuedWorkers) {
  ASSERT_TRUE(WaitForProcessCountToBe(1, 0));
  RunTest(FilePath(FILE_PATH_LITERAL("terminate_queued_workers.html")), "");
  // Make sure all workers exit.
  ASSERT_TRUE(WaitForProcessCountToBe(1, 0));
}

#if defined(OS_LINUX)
// http://crbug.com/30021
#define IncognitoSharedWorkers FLAKY_IncognitoSharedWorkers
#endif
// Incognito windows should not share workers with non-incognito windows
TEST_F(WorkerTest, IncognitoSharedWorkers) {
  // Load a non-incognito tab and have it create a shared worker
  RunTest(FilePath(FILE_PATH_LITERAL("incognito_worker.html")), "");
  // Incognito worker should not share with non-incognito
  RunIncognitoTest(FilePath(FILE_PATH_LITERAL("incognito_worker.html")));
}

const FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/workers");

#if defined(OS_WIN)
// http://crbug.com/33344 - NavigateAndWaitForAuth times out on the Windows
// build bots.
#define WorkerHttpAuth DISABLED_WorkerHttpAuth
#endif
// Make sure that auth dialog is displayed from worker context.
TEST_F(WorkerTest, WorkerHttpAuth) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url = test_server.GetURL("files/worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
}

#if defined(OS_WIN)
// http://crbug.com/33344 - NavigateAndWaitForAuth times out on the Windows
// build bots.
#define SharedWorkerHttpAuth DISABLED_SharedWorkerHttpAuth
#endif
// Make sure that auth dialog is displayed from shared worker context.
TEST_F(WorkerTest, SharedWorkerHttpAuth) {
  net::TestServer test_server(net::TestServer::TYPE_HTTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  GURL url = test_server.GetURL("files/shared_worker_auth.html");
  EXPECT_TRUE(NavigateAndWaitForAuth(tab, url));
  // TODO(atwilson): Add support to automation framework to test for auth
  // dialogs displayed by non-navigating tabs.
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_StressJSExecution) {
  RunWorkerFastLayoutTest("stress-js-execution.html");
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_UseMachineStack) {
  RunWorkerFastLayoutTest("use-machine-stack.html");
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_WorkerCall) {
  RunWorkerFastLayoutTest("worker-call.html");
}

// Crashy, http://crbug.com/35965.
// Flaky, http://crbug.com/36555.
TEST_F(WorkerTest, DISABLED_WorkerClonePort) {
  RunWorkerFastLayoutTest("worker-cloneport.html");
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_WorkerCloseFast) {
  RunWorkerFastLayoutTest("worker-close.html");
}

// http://crbug.com/84203.
TEST_F(WorkerTest, FLAKY_WorkerConstructor) {
  RunWorkerFastLayoutTest("worker-constructor.html");
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_WorkerContextGc) {
  RunWorkerFastLayoutTest("worker-context-gc.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerContextMultiPort) {
  RunWorkerFastLayoutTest("worker-context-multi-port.html");
}

// http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_WorkerEventListener) {
  RunWorkerFastLayoutTest("worker-event-listener.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerGC) {
  RunWorkerFastLayoutTest("worker-gc.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerInit) {
  RunWorkerFastLayoutTest("worker-init.html");
}

// worker-lifecycle.html relies on layoutTestController.workerThreadCount
// which is not currently implemented. http://crbug.com/45168
TEST_F(WorkerTest, DISABLED_WorkerLifecycle) {
  RunWorkerFastLayoutTest("worker-lifecycle.html");
}

#if defined(OS_WIN) || defined(OS_LINUX)
// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
#define WorkerLocation FLAKY_WorkerLocation
#endif
TEST_F(WorkerTest, WorkerLocation) {
  RunWorkerFastLayoutTest("worker-location.html");
}

// Flaky, http://crbug.com/71518.
TEST_F(WorkerTest, FLAKY_WorkerMapGc) {
  RunWorkerFastLayoutTest("wrapper-map-gc.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerMessagePort) {
  RunWorkerFastLayoutTest("worker-messageport.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerMessagePortGC) {
  RunWorkerFastLayoutTest("worker-messageport-gc.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerMultiPort) {
  RunWorkerFastLayoutTest("worker-multi-port.html");
}

// Flaky, http://crbug.com/76426.
TEST_F(WorkerTest, FLAKY_WorkerNavigator) {
  RunWorkerFastLayoutTest("worker-navigator.html");
}

#if defined(OS_WIN) || defined(OS_LINUX)
// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
#define WorkerReplaceGlobalConstructor FLAKY_WorkerReplaceGlobalConstructor
#endif
TEST_F(WorkerTest, WorkerReplaceGlobalConstructor) {
  RunWorkerFastLayoutTest("worker-replace-global-constructor.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerReplaceSelf) {
  RunWorkerFastLayoutTest("worker-replace-self.html");
}

// Mac: http://crbug.com/44457
// Others: http://crbug.com/101996
TEST_F(WorkerTest, FLAKY_WorkerScriptError) {
  RunWorkerFastLayoutTest("worker-script-error.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerTerminate) {
  RunWorkerFastLayoutTest("worker-terminate.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerTest, FLAKY_WorkerTimeout) {
  RunWorkerFastLayoutTest("worker-timeout.html");
}

//
// SharedWorkerFastLayoutTests
//
// http://crbug.com/27636 - incorrect URL_MISMATCH exceptions sometimes get
// generated on the windows try bots. FLAKY on Win.
// http://crbug.com/28445 - flakiness on mac
TEST_F(WorkerTest, FLAKY_SharedWorkerFastConstructor) {
  RunWorkerFastLayoutTest("shared-worker-constructor.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastContextGC) {
  RunWorkerFastLayoutTest("shared-worker-context-gc.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastEventListener) {
  RunWorkerFastLayoutTest("shared-worker-event-listener.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastException) {
  RunWorkerFastLayoutTest("shared-worker-exception.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastGC) {
  RunWorkerFastLayoutTest("shared-worker-gc.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastInIframe) {
  RunWorkerFastLayoutTest("shared-worker-in-iframe.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastLoadError) {
  RunWorkerFastLayoutTest("shared-worker-load-error.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastLocation) {
  RunWorkerFastLayoutTest("shared-worker-location.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastName) {
  RunWorkerFastLayoutTest("shared-worker-name.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastNavigator) {
  RunWorkerFastLayoutTest("shared-worker-navigator.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastReplaceGlobalConstructor) {
  RunWorkerFastLayoutTest("shared-worker-replace-global-constructor.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastReplaceSelf) {
  RunWorkerFastLayoutTest("shared-worker-replace-self.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastScriptError) {
  RunWorkerFastLayoutTest("shared-worker-script-error.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastShared) {
  RunWorkerFastLayoutTest("shared-worker-shared.html");
}

TEST_F(WorkerTest, FLAKY_SharedWorkerFastSimple) {
  RunWorkerFastLayoutTest("shared-worker-simple.html");
}

// http://crbug.com/16934
TEST_F(WorkerTest, DISABLED_WorkerHttpLayoutTests) {
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
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, kHttpPort);

  LayoutTestHttpServer http_server(new_http_root_dir_, kHttpPort);
  ASSERT_TRUE(http_server.Start());
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  ASSERT_TRUE(http_server.Stop());
}

TEST_F(WorkerTest, WorkerWebSocketLayoutTests) {
  static const char* kLayoutTestFiles[] = {
    "close-in-onmessage-crash.html",
    "close-in-shared-worker.html",
    "close-in-worker.html",
    "shared-worker-simple.html",
    "worker-handshake-challenge-randomness.html",
    "worker-simple.html"
  };

  FilePath websocket_test_dir;
  websocket_test_dir = websocket_test_dir.AppendASCII("http");
  websocket_test_dir = websocket_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("websocket");
  worker_test_dir = worker_test_dir.AppendASCII("tests");
  worker_test_dir = worker_test_dir.AppendASCII("hybi");
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(websocket_test_dir, worker_test_dir, kHttpPort);

  FilePath websocket_root_dir(temp_test_dir_);
  websocket_root_dir = websocket_root_dir.AppendASCII("LayoutTests");
  ui_test_utils::TestWebSocketServer websocket_server;
  ASSERT_TRUE(websocket_server.Start(websocket_root_dir));

  LayoutTestHttpServer http_server(new_http_root_dir_, kHttpPort);
  ASSERT_TRUE(http_server.Start());
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  ASSERT_TRUE(http_server.Stop());
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

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath worker_test_dir;
  worker_test_dir = worker_test_dir.AppendASCII("xmlhttprequest");
  worker_test_dir = worker_test_dir.AppendASCII("workers");
  InitializeForLayoutTest(http_test_dir, worker_test_dir, kHttpPort);

  LayoutTestHttpServer http_server(new_http_root_dir_, kHttpPort);
  ASSERT_TRUE(http_server.Start());
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  ASSERT_TRUE(http_server.Stop());
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
    // "message-channel-listener-circular-ownership.html",
  };

  FilePath fast_test_dir;
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

#if defined(OS_MACOSX)
// See http://crbug.com/48664
#define MAYBE_LimitPerPage DISABLED_LimitPerPage
#else
// See http://crbug.com/36800 for Windows/Linux
#define MAYBE_LimitPerPage FLAKY_LimitPerPage
#endif
TEST_F(WorkerTest, MAYBE_LimitPerPage) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                       FilePath(kManyWorkersFile));
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab + 1));

  NavigateToURL(url);
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
}

// Doesn't crash, but on all platforms, it sometimes fails.
// Flaky on all platforms: http://crbug.com/28445
// Hangs on Linux: http://crbug.com/30332
// Possibly causing ui_tests to hang on Mac: http://crbug.com/88958
// Times out consistently on all platforms.
TEST_F(WorkerTest, DISABLED_LimitTotal) {
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  int total_workers = WorkerService::kMaxWorkersWhenSeparate;

  int tab_count = (total_workers / max_workers_per_tab) + 1;
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                       FilePath(kManyWorkersFile));
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
  const FilePath::CharType* kGoogleDir = FILE_PATH_LITERAL("google");
  const FilePath::CharType* kGoogleFile = FILE_PATH_LITERAL("google.html");
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
      tab->NavigateToURL(ui_test_utils::GetTestUrl(FilePath(kGoogleDir),
                                                   FilePath(kGoogleFile))));

  ASSERT_TRUE(WaitForProcessCountToBe(tab_count, total_workers));
}

// Flaky, http://crbug.com/59786.
TEST_F(WorkerTest, FLAKY_WorkerClose) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                       FilePath(kWorkerClose));
  ASSERT_TRUE(tab->NavigateToURL(url));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(1, 0));
}

// Flaky, http://crbug.com/70861.
TEST_F(WorkerTest, FLAKY_QueuedSharedWorkerShutdown) {
  // Tests to make sure that queued shared workers are started up when
  // shared workers shut down.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
      FilePath(kQuerySharedWorkerShutdownFile));
  url = GURL(url.spec() + StringPrintf("?count=%d", max_workers_per_tab));

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  ASSERT_TRUE(tab->NavigateToURL(url));
  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(1, max_workers_per_tab));
}

// Flaky, http://crbug.com/69881.
TEST_F(WorkerTest, FLAKY_MultipleTabsQueuedSharedWorker) {
  // Tests to make sure that only one instance of queued shared workers are
  // started up even when those instances are on multiple tabs.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                       FilePath(kManySharedWorkersFile));
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
  GURL url2 = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                        FilePath(kShutdownSharedWorkerFile));
  url2 = GURL(url2.spec() + "?id=0");
  ASSERT_TRUE(window->AppendTab(url2));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(3, max_workers_per_tab));
}

// Flaky: http://crbug.com/48148
TEST_F(WorkerTest, FLAKY_QueuedSharedWorkerStartedFromOtherTab) {
  // Tests to make sure that queued shared workers are started up when
  // an instance is launched from another tab.
  int max_workers_per_tab = WorkerService::kMaxWorkersPerTabWhenSeparate;
  GURL url = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                       FilePath(kManySharedWorkersFile));
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
  GURL url2 = ui_test_utils::GetTestUrl(FilePath(kTestDir),
                                        FilePath(kSingleSharedWorkersFile));
  url2 = GURL(url2.spec() + StringPrintf("?id=%d", max_workers_per_tab));
  ASSERT_TRUE(window->AppendTab(url2));

  std::string value = WaitUntilCookieNonEmpty(tab.get(), url,
      kTestCompleteCookie, TestTimeouts::action_max_timeout_ms());
  ASSERT_STREQ(kTestCompleteSuccess, value.c_str());
  ASSERT_TRUE(WaitForProcessCountToBe(2, max_workers_per_tab+1));
}

class WorkerFileSystemTest : public WorkerTest {
 protected:
  void RunWorkerFileSystemLayoutTest(const std::string& test_case_file_name) {
    FilePath worker_test_dir;
    worker_test_dir = worker_test_dir.AppendASCII("fast");

    FilePath filesystem_test_dir;
    filesystem_test_dir = filesystem_test_dir.AppendASCII("filesystem");
    filesystem_test_dir = filesystem_test_dir.AppendASCII("workers");
    InitializeForLayoutTest(worker_test_dir, filesystem_test_dir, kNoHttpPort);

    FilePath resource_dir;
    resource_dir = resource_dir.AppendASCII("resources");
    AddResourceForLayoutTest(worker_test_dir.AppendASCII("filesystem"),
                             resource_dir);

    // FS tests also rely on common files in js/resources.
    FilePath js_dir = worker_test_dir.AppendASCII("js");
    FilePath js_resource_dir;
    js_resource_dir = js_resource_dir.AppendASCII("resources");
    AddResourceForLayoutTest(js_dir, js_resource_dir);

    RunLayoutTest(test_case_file_name, kNoHttpPort);

    // Navigate to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    GURL about_url(chrome::kAboutBlankURL);
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(about_url));
  }

  void RunWorkerFileSystemLayoutHttpTests(const char* tests[], int num_tests) {
    FilePath worker_test_dir = FilePath().AppendASCII("http")
                                         .AppendASCII("tests");

    FilePath filesystem_test_dir = FilePath().AppendASCII("filesystem")
                                             .AppendASCII("workers");
    InitializeForLayoutTest(worker_test_dir, filesystem_test_dir, kHttpPort);

    AddResourceForLayoutTest(worker_test_dir.AppendASCII("filesystem"),
                             FilePath().AppendASCII("resources"));

    AddResourceForLayoutTest(worker_test_dir.AppendASCII("filesystem"),
                             FilePath().AppendASCII("script-tests"));

    AddResourceForLayoutTest(worker_test_dir.AppendASCII("filesystem"),
                             FilePath().AppendASCII("workers")
                                       .AppendASCII("script-tests"));

    LayoutTestHttpServer http_server(new_http_root_dir_, 8000);
    ASSERT_TRUE(http_server.Start());
    for (int i = 0; i < num_tests; ++i)
      RunLayoutTest(tests[i], 8000);
    ASSERT_TRUE(http_server.Stop());

    // Navigate to a blank page so that any workers are cleaned up.
    // This helps leaks trackers do a better job of reporting.
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    GURL about_url(chrome::kAboutBlankURL);
    EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(about_url));
  }
};

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerFileSystemTest, FLAKY_Temporary) {
  RunWorkerFileSystemLayoutTest("simple-temporary.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerFileSystemTest, FLAKY_Persistent) {
  RunWorkerFileSystemLayoutTest("simple-persistent.html");
}

#if defined(OS_LINUX)
// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
#define SyncTemporary FLAKY_SyncTemporary
#endif
TEST_F(WorkerFileSystemTest, SyncTemporary) {
  RunWorkerFileSystemLayoutTest("simple-temporary-sync.html");
}

// TODO(dpranke): This started failing in the webkit roll 84046:84325.
// The upstream expectation needs to be updated.
TEST_F(WorkerFileSystemTest, FAILS_SyncPersistent) {
  RunWorkerFileSystemLayoutTest("simple-persistent-sync.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerFileSystemTest, FLAKY_AsyncOperations) {
  RunWorkerFileSystemLayoutTest("async-operations.html");
}

#if defined(OS_LINUX) || defined(OS_MACOSX)
// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
#define SyncOperations FLAKY_SyncOperations
#endif
TEST_F(WorkerFileSystemTest, SyncOperations) {
  RunWorkerFileSystemLayoutTest("sync-operations.html");
}

// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
TEST_F(WorkerFileSystemTest, FLAKY_FileEntryToURISync) {
  RunWorkerFileSystemLayoutTest("file-entry-to-uri-sync.html");
}

// http://crbug.com/77442
TEST_F(WorkerFileSystemTest, FLAKY_ResolveURLHttpTests) {
  static const char* kLayoutTests[] = {
    "resolve-url.html",
    "resolve-url-sync.html"
  };
  RunWorkerFileSystemLayoutHttpTests(kLayoutTests, arraysize(kLayoutTests));
}

#if defined(OS_LINUX)
// Fails on Linux due to an assert in WebKit's RNG.
// See http://webkit.org/b/55728.
#define FileFromFileEntry DISABLED_FileFromFileEntry
#else
// http://crbug.com/101996
#define FileFromFileEntry FLAKY_FileFromFileEntry
#endif
TEST_F(WorkerFileSystemTest, FileFromFileEntry) {
  RunWorkerFileSystemLayoutTest("file-from-file-entry.html");
}

#if defined(OS_LINUX)
// Fails on Linux due to an assert in WebKit's RNG.
// See http://webkit.org/b/55728.
#define FileFromFileEntrySync DISABLED_FileFromFileEntrySync
#else
// http://crbug.com/101996 (started flaking with WebKit roll 98537:98582).
#define FileFromFileEntrySync FLAKY_FileFromFileEntrySync
#endif
TEST_F(WorkerFileSystemTest, FileFromFileEntrySync) {
  RunWorkerFileSystemLayoutTest("file-from-file-entry-sync.html");
}

#if defined(OS_LINUX)
// Fails on Linux due to an assert in WebKit's RNG.
// See http://webkit.org/b/55728.
#define FileWriterTruncateExtend DISABLED_FileWriterTruncateExtend
#endif
TEST_F(WorkerFileSystemTest, FileWriterTruncateExtend) {
  RunWorkerFileSystemLayoutTest("file-writer-truncate-extend.html");
}

#if defined(OS_LINUX)
// Fails on Linux due to an assert in WebKit's RNG.
// See http://webkit.org/b/55728.
#define FileWriterSyncTruncateExtend DISABLED_FileWriterSyncTruncateExtend
#endif
TEST_F(WorkerFileSystemTest, FileWriterSyncTruncateExtend) {
  RunWorkerFileSystemLayoutTest("file-writer-sync-truncate-extend.html");
}

#if defined(OS_LINUX)
// Fails on Linux due to an assert in WebKit's RNG.
// See http://webkit.org/b/55728.
#define FileWriterSyncWriteOverlapped DISABLED_FileWriterSyncWriteOverlapped
#endif
TEST_F(WorkerFileSystemTest, FileWriterSyncWriteOverlapped) {
  RunWorkerFileSystemLayoutTest("file-writer-sync-write-overlapped.html");
}
