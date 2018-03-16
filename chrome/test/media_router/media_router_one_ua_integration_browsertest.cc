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

class MediaRouterIntegrationOneUABrowserTest
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

// TODO(crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest, MANUAL_Basic) {
  RunBasicTest();
}

// TODO(crbug.com/822216): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_SendAndOnMessage) {
  RunSendMessageTest("foo");
}

// TODO(crbug.com/821717): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_ReceiverCloseConnection) {
  WebContents* web_contents = StartSessionWithTestPageAndChooseSink();
  CheckSessionValidity(web_contents);
  ExecuteJavaScriptAPI(web_contents, kInitiateCloseFromReceiverPageScript);
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       Fail_SendMessage) {
  RunFailToSendMessageTest();
}

// TODO(crbug.com/821717): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_ReconnectSession) {
  RunReconnectSessionTest();
}

// TODO(crbug.com/821717): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUABrowserTest,
                       MANUAL_ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

class MediaRouterIntegrationOneUANoReceiverBrowserTest
    : public MediaRouterIntegrationBrowserTest {
 public:
  GURL GetTestPageUrl(const base::FilePath& full_path) override {
    GURL url = MediaRouterIntegrationBrowserTest::GetTestPageUrl(full_path);
    return GURL(url.spec() + "?__oneUANoReceiver__=true");
  }
};

// TODO(crbug.com/822179,822337): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MANUAL_Basic) {
  RunBasicTest();
}

// TODO(crbug.com/821717): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       MANUAL_Fail_SendMessage) {
  RunFailToSendMessageTest();
}

// TODO(crbug.com/822231): Flaky in Chromium waterfall.
IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       ReconnectSession) {
  RunReconnectSessionTest();
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationOneUANoReceiverBrowserTest,
                       ReconnectSessionSameTab) {
  RunReconnectSessionSameTabTest();
}

}  // namespace media_router
