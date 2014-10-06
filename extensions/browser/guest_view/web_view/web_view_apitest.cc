// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/guest_view/web_view/test_guest_view_manager.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kRedirectResponseFullPath[] = "/guest_redirect.html";
const char kTestDataDirectory[] = "testDataDirectory";
const char kTestServerPort[] = "testServer.port";
const char kTestWebSocketPort[] = "testWebSocketPort";

class EmptyHttpResponse : public net::test_server::HttpResponse {
 public:
  virtual std::string ToResponseString() const override {
    return std::string();
  }
};

// Handles |request| by serving a redirect response.
scoped_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
    const std::string& path,
    const GURL& redirect_target,
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(path, request.relative_url, true))
    return scoped_ptr<net::test_server::HttpResponse>();

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return http_response.PassAs<net::test_server::HttpResponse>();
}

// Handles |request| by serving an empty response.
scoped_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (StartsWithASCII(path, request.relative_url, true)) {
    return scoped_ptr<net::test_server::HttpResponse>(new EmptyHttpResponse);
  }

  return scoped_ptr<net::test_server::HttpResponse>();
}

}  // namespace

namespace extensions {

// This class intercepts download request from the guest.
class WebViewAPITest : public AppShellTest {
 protected:
  void LaunchApp(const std::string& app_location) {
    base::FilePath test_data_dir;
    PathService::Get(DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(app_location.c_str());

    test_config_.SetString(kTestDataDirectory,
                           net::FilePathToFileURL(test_data_dir).spec());

    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    ASSERT_TRUE(extension_system_->LoadApp(test_data_dir));
    extension_system_->LaunchApp();

    ExtensionTestMessageListener launch_listener("LAUNCHED", false);
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(browser_context_)->app_windows();
    DCHECK(app_window_list.size() == 1);
    embedder_web_contents_ = (*app_window_list.begin())->web_contents();
  }

  void RunTest(const std::string& test_name, const std::string& app_location) {
    LaunchApp(app_location);

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            embedder_web_contents_,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "Unable to start test.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  virtual void SetUpOnMainThread() override {
    AppShellTest::SetUpOnMainThread();

    TestGetConfigFunction::set_test_config_state(&test_config_);
    base::FilePath test_data_dir;
    test_config_.SetInteger(kTestWebSocketPort, 0);
  }

  void StartTestServer() {
    // For serving guest pages.
    if (!embedded_test_server()->InitializeAndWaitUntilReady()) {
      LOG(ERROR) << "Failed to start test server.";
      return;
    }

    test_config_.SetInteger(kTestServerPort, embedded_test_server()->port());

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&RedirectResponseHandler,
                   kRedirectResponsePath,
                   embedded_test_server()->GetURL(kRedirectResponseFullPath)));

    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&EmptyResponseHandler, kEmptyResponsePath));
  }

  void StopTestServer() {
    if (!embedded_test_server()->ShutdownAndWaitUntilComplete()) {
      LOG(ERROR) << "Failed to shutdown test server.";
    }
  }

  virtual void TearDownOnMainThread() override {
    TestGetConfigFunction::set_test_config_state(nullptr);

    AppShellTest::TearDownOnMainThread();
  }

  TestGuestViewManager* GetGuestViewManager() {
    return static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(
            ShellContentBrowserClient::Get()->GetBrowserContext()));
  }

  WebViewAPITest() { GuestViewManager::set_factory_for_testing(&factory_); }

 protected:
  content::WebContents* embedder_web_contents_;
  TestGuestViewManagerFactory factory_;
  base::DictionaryValue test_config_;
};

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAllowTransparencyAttribute) {
  RunTest("testAllowTransparencyAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAPIMethodExistence) {
  RunTest("testAPIMethodExistence", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAssignSrcAfterCrash) {
  RunTest("testAssignSrcAfterCrash", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeAfterNavigation) {
  RunTest("testAutosizeAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeBeforeNavigation) {
  RunTest("testAutosizeBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeWithPartialAttributes) {
  RunTest("testAutosizeWithPartialAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCannotMutateEventName) {
  RunTest("testCannotMutateEventName", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestChromeExtensionRelativePath) {
  RunTest("testChromeExtensionRelativePath", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestChromeExtensionURL) {
  RunTest("testChromeExtensionURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestContentLoadEvent) {
  RunTest("testContentLoadEvent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDeclarativeWebRequestAPI) {
  StartTestServer();
  RunTest("testDeclarativeWebRequestAPI", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestDeclarativeWebRequestAPISendMessage) {
  StartTestServer();
  RunTest("testDeclarativeWebRequestAPISendMessage", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDestroyOnEventListener) {
  RunTest("testDestroyOnEventListener", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDisplayNoneWebviewLoad) {
  RunTest("testDisplayNoneWebviewLoad", "web_view/apitest");
}

// TODO(lfg): Re-enable this test once the fix for webview leaking
// RenderProcessHost has landed. See http://crbug.com/419020 .
IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       DISABLED_TestDisplayNoneWebviewRemoveChild) {
  RunTest("testDisplayNoneWebviewRemoveChild", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestEventName) {
  RunTest("testEventName", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestExecuteScript) {
  RunTest("testExecuteScript", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestExecuteScriptFail) {
  RunTest("testExecuteScriptFail", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
  RunTest("testExecuteScriptIsAbortedWhenWebViewSourceIsChanged",
          "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestFindAPI) {
  RunTest("testFindAPI", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestFindAPI_findupdate) {
  RunTest("testFindAPI_findupdate", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestGetProcessId) {
  RunTest("testGetProcessId", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestHiddenBeforeNavigation) {
  RunTest("testHiddenBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestInlineScriptFromAccessibleResources) {
  RunTest("testInlineScriptFromAccessibleResources", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestInvalidChromeExtensionURL) {
  RunTest("testInvalidChromeExtensionURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestLoadAbortChromeExtensionURLWrongPartition) {
  RunTest("testLoadAbortChromeExtensionURLWrongPartition", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortEmptyResponse) {
  StartTestServer();
  RunTest("testLoadAbortEmptyResponse", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalChromeURL) {
  RunTest("testLoadAbortIllegalChromeURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalFileURL) {
  RunTest("testLoadAbortIllegalFileURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortIllegalJavaScriptURL) {
  RunTest("testLoadAbortIllegalJavaScriptURL", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortInvalidNavigation) {
  RunTest("testLoadAbortInvalidNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadAbortNonWebSafeScheme) {
  RunTest("testLoadAbortNonWebSafeScheme", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadProgressEvent) {
  RunTest("testLoadProgressEvent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadStartLoadRedirect) {
  StartTestServer();
  RunTest("testLoadStartLoadRedirect", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavigateAfterResize) {
  RunTest("testNavigateAfterResize", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavigationToExternalProtocol) {
  RunTest("testNavigationToExternalProtocol", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestNavOnConsecutiveSrcAttributeChanges) {
  RunTest("testNavOnConsecutiveSrcAttributeChanges", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavOnSrcAttributeChange) {
  RunTest("testNavOnSrcAttributeChange", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindow) {
  StartTestServer();
  RunTest("testNewWindow", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoPreventDefault) {
  StartTestServer();
  RunTest("testNewWindowNoPreventDefault", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoReferrerLink) {
  StartTestServer();
  RunTest("testNewWindowNoReferrerLink", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowTwoListeners) {
  StartTestServer();
  RunTest("testNewWindowTwoListeners", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestOnEventProperty) {
  RunTest("testOnEventProperties", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestPartitionRaisesException) {
  RunTest("testPartitionRaisesException", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestPartitionRemovalAfterNavigationFails) {
  RunTest("testPartitionRemovalAfterNavigationFails", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReassignSrcAttribute) {
  RunTest("testReassignSrcAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveWebviewOnExit) {
  StartTestServer();

  // Launch the app and wait until it's ready to load a test.
  LaunchApp("web_view/apitest");

  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded", false);
  EXPECT_TRUE(content::ExecuteScript(embedder_web_contents_,
                                     "runTest('testRemoveWebviewOnExit')"));

  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForGuestCreated();
  EXPECT_TRUE(guest_web_contents->GetRenderProcessHost()->IsIsolatedGuest());
  ASSERT_TRUE(guest_loaded_listener.WaitUntilSatisfied());

  content::WebContentsDestroyedWatcher destroyed_watcher(guest_web_contents);

  // Tell the embedder to kill the guest.
  EXPECT_TRUE(content::ExecuteScript(embedder_web_contents_,
                                     "removeWebviewOnExitDoCrash()"));

  // Wait until the guest WebContents is destroyed.
  destroyed_watcher.Wait();
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReload) {
  RunTest("testReload", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReloadAfterTerminate) {
  RunTest("testReloadAfterTerminate", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveSrcAttribute) {
  RunTest("testRemoveSrcAttribute", "web_view/apitest");
}

// TODO(lfg): Re-enable this test once the fix for webview leaking
// RenderProcessHost has landed. See http://crbug.com/419020 .
IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       DISABLED_TestRemoveWebviewAfterNavigation) {
  RunTest("testRemoveWebviewAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestResizeWebviewResizesContent) {
  RunTest("testResizeWebviewResizesContent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestTerminateAfterExit) {
  RunTest("testTerminateAfterExit", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPI) {
  StartTestServer();
  RunTest("testWebRequestAPI", "web_view/apitest");
  StopTestServer();
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIExistence) {
  StartTestServer();
  RunTest("testWebRequestAPIExistence", "web_view/apitest");
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIGoogleProperty) {
  StartTestServer();
  RunTest("testWebRequestAPIGoogleProperty", "web_view/apitest");
  StopTestServer();
}

}  // namespace extensions
