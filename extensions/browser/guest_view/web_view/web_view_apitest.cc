// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_apitest.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/switches.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/display/display_switches.h"

using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;

namespace {

const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kRedirectResponseFullPath[] = "/guest_redirect.html";
const char kUserAgentRedirectResponsePath[] = "/detect-user-agent";
const char kTestServerPort[] = "testServer.port";

// Handles |request| by serving a redirect response if the |User-Agent| is
// foobar.
static std::unique_ptr<net::test_server::HttpResponse> UserAgentResponseHandler(
    const std::string& path,
    const GURL& redirect_target,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  auto it = request.headers.find("User-Agent");
  EXPECT_TRUE(it != request.headers.end());
  if (!base::StartsWith("foobar", it->second, base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

class WebContentsHiddenObserver : public content::WebContentsObserver {
 public:
  WebContentsHiddenObserver(content::WebContents* web_contents,
                            const base::Closure& hidden_callback)
      : WebContentsObserver(web_contents),
        hidden_callback_(hidden_callback),
        hidden_observed_(false) {
  }

  // WebContentsObserver.
  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN) {
      hidden_observed_ = true;
      hidden_callback_.Run();
    }
  }

  bool hidden_observed() { return hidden_observed_; }

 private:
  base::Closure hidden_callback_;
  bool hidden_observed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsHiddenObserver);
};

// Handles |request| by serving a redirect response.
std::unique_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
    const std::string& path,
    const GURL& redirect_target,
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(path, request.relative_url,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

// Handles |request| by serving an empty response.
std::unique_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
    const std::string& path,
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(path, request.relative_url,
                       base::CompareCase::SENSITIVE)) {
    return std::unique_ptr<net::test_server::HttpResponse>(
        new net::test_server::RawHttpResponse("", ""));
  }

  return std::unique_ptr<net::test_server::HttpResponse>();
}

}  // namespace

namespace extensions {

WebViewAPITest::WebViewAPITest() {
  GuestViewManager::set_factory_for_testing(&factory_);
}

void WebViewAPITest::LaunchApp(const std::string& app_location) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  PathService::Get(DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_location.c_str());

  const Extension* extension = extension_system_->LoadApp(test_data_dir);
  ASSERT_TRUE(extension);
  extension_system_->LaunchApp(extension->id());

  ExtensionTestMessageListener launch_listener("LAUNCHED", false);
  launch_listener.set_failure_message("FAILURE");
  ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

  embedder_web_contents_ = GetFirstAppWindowWebContents();
}

content::WebContents* WebViewAPITest::GetFirstAppWindowWebContents() {
  const AppWindowRegistry::AppWindowList& app_window_list =
      AppWindowRegistry::Get(browser_context_)->app_windows();
  DCHECK_EQ(1u, app_window_list.size());
  return (*app_window_list.begin())->web_contents();
}

void WebViewAPITest::RunTest(const std::string& test_name,
                             const std::string& app_location) {
  LaunchApp(app_location);

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  ASSERT_TRUE(content::ExecuteScript(
      embedder_web_contents_,
      base::StringPrintf("runTest('%s')", test_name.c_str())))
      << "Unable to start test.";
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());
}

void WebViewAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  AppShellTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(::switches::kJavaScriptFlags, "--expose-gc");
}

void WebViewAPITest::SetUpOnMainThread() {
  AppShellTest::SetUpOnMainThread();

  TestGetConfigFunction::set_test_config_state(&test_config_);
}

void WebViewAPITest::StartTestServer(const std::string& app_location) {
  // For serving guest pages.
  if (!embedded_test_server()->InitializeAndListen()) {
    LOG(ERROR) << "Failed to start test server.";
    return;
  }

  test_config_.SetInteger(kTestServerPort, embedded_test_server()->port());

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  PathService::Get(DIR_TEST_DATA, &test_data_dir);
  test_data_dir = test_data_dir.AppendASCII(app_location.c_str());
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&RedirectResponseHandler,
                 kRedirectResponsePath,
                 embedded_test_server()->GetURL(kRedirectResponseFullPath)));

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&EmptyResponseHandler, kEmptyResponsePath));

  embedded_test_server()->RegisterRequestHandler(
      base::Bind(
          &UserAgentResponseHandler,
          kUserAgentRedirectResponsePath,
          embedded_test_server()->GetURL(kRedirectResponseFullPath)));

  embedded_test_server()->StartAcceptingConnections();
}

void WebViewAPITest::StopTestServer() {
  if (!embedded_test_server()->ShutdownAndWaitUntilComplete()) {
    LOG(ERROR) << "Failed to shutdown test server.";
  }
}

void WebViewAPITest::TearDownOnMainThread() {
  DesktopController::instance()->CloseAppWindows();
  GetGuestViewManager()->WaitForAllGuestsDeleted();
  TestGetConfigFunction::set_test_config_state(nullptr);

  AppShellTest::TearDownOnMainThread();
}

void WebViewAPITest::SendMessageToEmbedder(const std::string& message) {
  EXPECT_TRUE(
      content::ExecuteScript(
          GetEmbedderWebContents(),
          base::StringPrintf("onAppCommand('%s');", message.c_str())));
}

content::WebContents* WebViewAPITest::GetEmbedderWebContents() {
  if (!embedder_web_contents_)
    embedder_web_contents_ = GetFirstAppWindowWebContents();
  return embedder_web_contents_;
}

TestGuestViewManager* WebViewAPITest::GetGuestViewManager() {
  content::BrowserContext* context =
      ShellContentBrowserClient::Get()->GetBrowserContext();
  TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
      TestGuestViewManager::FromBrowserContext(context));
  // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
  // before a guest is created.
  if (!manager) {
    manager =
        static_cast<TestGuestViewManager*>(GuestViewManager::CreateWithDelegate(
            context,
            ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                context)));
  }
  return manager;
}

void WebViewAPITest::SendMessageToGuestAndWait(
    const std::string& message,
    const std::string& wait_message) {
  std::unique_ptr<ExtensionTestMessageListener> listener;
  if (!wait_message.empty())
    listener.reset(new ExtensionTestMessageListener(wait_message, false));

  EXPECT_TRUE(
      content::ExecuteScript(
          GetGuestWebContents(),
          base::StringPrintf("onAppCommand('%s');", message.c_str())));

  if (listener)
    ASSERT_TRUE(listener->WaitUntilSatisfied());
}

void WebViewDPIAPITest::SetUp() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(::switches::kForceDeviceScaleFactor,
                                  base::StringPrintf("%f", scale()));
  WebViewAPITest::SetUp();
}

content::WebContents* WebViewAPITest::GetGuestWebContents() {
  return GetGuestViewManager()->WaitForSingleGuestCreated();
}

// Occasionally hits NOTIMPLEMENTED on Linux.  https://crbug.com/422998
#if defined(OS_LINUX)
#define MAYBE_AcceptTouchEvents DISABLED_AcceptTouchEvents
#else
#define MAYBE_AcceptTouchEvents AcceptTouchEvents
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_AcceptTouchEvents) {
  // This test only makes sense for non-OOPIF WebView, since with
  // GuestViewCrossProcessFrames events are routed directly to the
  // guest, so the embedder does not need to know about the installation of
  // touch handlers.
  if (base::FeatureList::IsEnabled(::features::kGuestViewCrossProcessFrames))
    return;

  LaunchApp("web_view/accept_touch_events");

  content::RenderViewHost* embedder_rvh =
      GetEmbedderWebContents()->GetRenderViewHost();

  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);

  SendMessageToGuestAndWait("install-touch-handler", "installed-touch-handler");

  // Note that we need to wait for the installed/registered touch handler to
  // appear in browser process before querying |embedder_rvh|.
  // In practice, since we do a roundrtip from browser process to guest and
  // back, this is sufficient.
  embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_TRUE(embedder_has_touch_handler);

  SendMessageToGuestAndWait("uninstall-touch-handler",
                            "uninstalled-touch-handler");
  // Same as the note above about waiting.
  embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, EmbedderVisibilityChanged) {
  LaunchApp("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-embedder");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// Test for http://crbug.com/419611.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, DisplayNoneSetSrc) {
  LaunchApp("web_view/display_none_set_src");
  // Navigate the guest while it's in "display: none" state.
  SendMessageToEmbedder("navigate-guest");
  GetGuestViewManager()->WaitForSingleGuestCreated();

  // Now attempt to navigate the guest again.
  SendMessageToEmbedder("navigate-guest");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);
  // Making the guest visible would trigger loadstop.
  SendMessageToEmbedder("show-guest");
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// This test verifies that hiding the guest triggers WebContents::WasHidden().
IN_PROC_BROWSER_TEST_F(WebViewAPITest, GuestVisibilityChanged) {
  LaunchApp("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-guest");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// http://crbug.com/291278
#if defined(OS_WIN)
#define MAYBE_CloseOnLoadcommit DISABLED_CloseOnLoadcommit
#else
#define MAYBE_CloseOnLoadcommit CloseOnLoadcommit
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_CloseOnLoadcommit) {
  LaunchApp("web_view/close_on_loadcommit");
  ExtensionTestMessageListener test_done_listener("done-close-on-loadcommit",
                                                  false);
  ASSERT_TRUE(test_done_listener.WaitUntilSatisfied());
}

// This test verifies that reloading the embedder reloads the guest (and doest
// not crash).
IN_PROC_BROWSER_TEST_F(WebViewAPITest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // app for this test.
  LaunchApp("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("LAUNCHED", false);
  embedder_web_contents_->GetController().Reload(content::ReloadType::NORMAL,
                                                 false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

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

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeBeforeNavigation) {
  RunTest("testAutosizeBeforeNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeHeight) {
  RunTest("testAutosizeHeight", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewDPIAPITest, TestAutosizeRemoveAttributes) {
  RunTest("testAutosizeRemoveAttributes", "web_view/apitest");
}

// http://crbug.com/473177
IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       DISABLED_TestAutosizeWithPartialAttributes) {
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
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testDeclarativeWebRequestAPI", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestDeclarativeWebRequestAPISendMessage) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testDeclarativeWebRequestAPISendMessage", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDestroyOnEventListener) {
  RunTest("testDestroyOnEventListener", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogAlert) {
  RunTest("testDialogAlert", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirm) {
  RunTest("testDialogConfirm", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmCancel) {
  RunTest("testDialogConfirmCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmDefaultCancel) {
  RunTest("testDialogConfirmDefaultCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogConfirmDefaultGCCancel) {
  RunTest("testDialogConfirmDefaultGCCancel", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDialogPrompt) {
  RunTest("testDialogPrompt", "web_view/dialog");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestDisplayNoneWebviewLoad) {
  RunTest("testDisplayNoneWebviewLoad", "web_view/apitest");
}

// Flaky. See http://crbug.com/769467.
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

// Flaky and likely not testing the right assertion.  https://crbug.com/702918
IN_PROC_BROWSER_TEST_F(
    WebViewAPITest,
    DISABLED_TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
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
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadAbortEmptyResponse", app_location);
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

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCanGoBack) {
  RunTest("testCanGoBack", "web_view/apitest");
}

// Crashes on Win only.  http://crbug.com/805903
#if defined(OS_WIN)
#define MAYBE_TestLoadStartLoadRedirect DISABLED_TestLoadStartLoadRedirect
#else
#define MAYBE_TestLoadStartLoadRedirect TestLoadStartLoadRedirect
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestLoadStartLoadRedirect) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadStartLoadRedirect", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavigateAfterResize) {
  RunTest("testNavigateAfterResize", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavigationToExternalProtocol) {
  RunTest("testNavigationToExternalProtocol", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestContentInitiatedNavigationToDataUrlBlocked) {
  RunTest("testContentInitiatedNavigationToDataUrlBlocked", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestNavOnConsecutiveSrcAttributeChanges) {
  RunTest("testNavOnConsecutiveSrcAttributeChanges", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNavOnSrcAttributeChange) {
  RunTest("testNavOnSrcAttributeChange", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindow) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindow", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoPreventDefault) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowNoPreventDefault", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowNoReferrerLink) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowNoReferrerLink", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestNewWindowTwoListeners) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testNewWindowTwoListeners", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestOnEventProperty) {
  RunTest("testOnEventProperties", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestPartitionChangeAfterNavigation) {
  RunTest("testPartitionChangeAfterNavigation", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest,
                       TestPartitionRemovalAfterNavigationFails) {
  RunTest("testPartitionRemovalAfterNavigationFails", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestReassignSrcAttribute) {
  RunTest("testReassignSrcAttribute", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveWebviewOnExit) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);

  // Launch the app and wait until it's ready to load a test.
  LaunchApp("web_view/apitest");

  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded", false);
  EXPECT_TRUE(content::ExecuteScript(embedder_web_contents_,
                                     "runTest('testRemoveWebviewOnExit')"));

  content::WebContents* guest_web_contents = GetGuestWebContents();
  EXPECT_TRUE(
      guest_web_contents->GetMainFrame()->GetProcess()->IsForGuestsOnly());
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

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestRemoveWebviewAfterNavigation) {
  RunTest("testRemoveWebviewAfterNavigation", "web_view/apitest");
}

#if defined(OS_WIN)
#define MAYBE_TestResizeWebviewResizesContent \
  DISABLED_TestResizeWebviewResizesContent
#else
#define MAYBE_TestResizeWebviewResizesContent TestResizeWebviewResizesContent
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestResizeWebviewResizesContent) {
  RunTest("testResizeWebviewResizesContent", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestTerminateAfterExit) {
  RunTest("testTerminateAfterExit", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPI) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testWebRequestAPI", app_location);
  StopTestServer();
}

// Crashes on Win only.  http://crbug.com/805903
#if defined(OS_WIN)
#define MAYBE_TestWebRequestAPIWithHeaders DISABLED_TestWebRequestAPIWithHeaders
#else
#define MAYBE_TestWebRequestAPIWithHeaders TestWebRequestAPIWithHeaders
#endif
IN_PROC_BROWSER_TEST_F(WebViewAPITest, MAYBE_TestWebRequestAPIWithHeaders) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testWebRequestAPIWithHeaders", app_location);
  StopTestServer();
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestLoadEventsSameDocumentNavigation) {
  std::string app_location = "web_view/apitest";
  StartTestServer(app_location);
  RunTest("testLoadEventsSameDocumentNavigation", app_location);
  StopTestServer();
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIExistence) {
  RunTest("testWebRequestAPIExistence", "web_view/apitest");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebRequestAPIGoogleProperty) {
  RunTest("testWebRequestAPIGoogleProperty", "web_view/apitest");
}

// This test verifies that webview.contentWindow works inside an iframe
IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestWebViewInsideFrame) {
  LaunchApp("web_view/inside_iframe");
}

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestCaptureVisibleRegion) {
  RunTest("testCaptureVisibleRegion", "web_view/apitest");
}

}  // namespace extensions
