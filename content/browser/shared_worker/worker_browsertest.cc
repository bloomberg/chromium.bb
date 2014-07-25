// Copyright 2014 The Chromium Authors. All rights reserved.
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
