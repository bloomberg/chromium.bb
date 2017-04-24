// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/test/media_router/media_router_integration_browsertest.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;

namespace media_router {

namespace {
const char kInitiateCloseFromReceiverPageScript[] =
    "initiateCloseFromReceiverPage();";
}

class MediaRouterOneUAIntegrationBrowserTest
    : public MediaRouterIntegrationBrowserTest {
 public:
  void SetUpOnMainThread() override {
    MediaRouterIntegrationBrowserTest::SetUpOnMainThread();

    // Set up embedded test server to serve offscreen presentation with relative
    // URL "presentation_receiver.html".
    base::FilePath base_dir;
    CHECK(PathService::Get(base::DIR_MODULE, &base_dir));
    base::FilePath resource_dir = base_dir.Append(
        FILE_PATH_LITERAL("media_router/browser_test_resources/"));
    embedded_test_server()->ServeFilesFromDirectory(resource_dir);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    GURL url = embedded_test_server()->GetURL("/basic_test.html");
    return GURL(url.spec() + "?__oneUA__=true");
  }
};

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest, MANUAL_Basic) {
  RunBasicTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest,
                       MANUAL_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest,
                       MANUAL_ReceiverCloseConnection) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kInitiateCloseFromReceiverPageScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest,
                       MANUAL_Fail_SendMessage) {
  RunFailToSendMessageTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest,
                       MANUAL_ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUAIntegrationBrowserTest,
                       MANUAL_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

class MediaRouterOneUANoReceiverIntegrationBrowserTest
    : public MediaRouterIntegrationBrowserTest {
 public:
  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    GURL url = MediaRouterIntegrationBrowserTest::GetTestPageUrl(full_path);
    return GURL(url.spec() + "?__oneUANoReceiver__=true");
  }
};

IN_PROC_BROWSER_TEST_F(MediaRouterOneUANoReceiverIntegrationBrowserTest,
                       MANUAL_Basic) {
  RunBasicTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUANoReceiverIntegrationBrowserTest,
                       MANUAL_Fail_SendMessage) {
  RunFailToSendMessageTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUANoReceiverIntegrationBrowserTest,
                       MANUAL_ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterOneUANoReceiverIntegrationBrowserTest,
                       MANUAL_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

}  // namespace media_router
