// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/features.h"

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::GuestViewManagerDelegate;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

// The test extension id is set by the key value in the manifest.
const char kExtensionId[] = "oickdpebdnfbgkcaoklfcdhjniefkcji";

class MimeHandlerViewTest : public extensions::ExtensionApiTest {
 public:
  MimeHandlerViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~MimeHandlerViewTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &MimeHandlerViewTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

  // TODO(paulmeyer): This function is implemented over and over by the
  // different GuestView test classes. It really needs to be refactored out to
  // some kind of GuestViewTest base class.
  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  const extensions::Extension* LoadTestExtension() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    CHECK_EQ(std::string(kExtensionId), extension->id());

    return extension;
  }

  void RunTestWithUrl(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::Bind(&TestMimeHandlerViewGuest::Create));

    const extensions::Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    extensions::ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(), url);

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  void RunTest(const std::string& path) {
    RunTestWithUrl(embedded_test_server()->GetURL("/" + path));
  }

  int basic_count() const { return basic_count_; }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/testBasic.csv")
      basic_count_++;
  }

  TestGuestViewManagerFactory factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
  int basic_count_ = 0;
};

// The parametric version of the test class which runs the test both on
// BrowserPlugin-based and cross-process-frame-based MimeHandlerView
// implementation. All current browser tests should eventually be moved to this
// and then eventually drop the BrowserPlugin dependency once
// https://crbug.com/659750 is fixed.
class MimeHandlerViewCrossProcessTest
    : public MimeHandlerViewTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MimeHandlerViewCrossProcessTest() : MimeHandlerViewTest() {}
  ~MimeHandlerViewCrossProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* cl) override {
    MimeHandlerViewTest::SetUpCommandLine(cl);
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kMimeHandlerViewInCrossProcessFrame);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewCrossProcessTest);
};

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        MimeHandlerViewCrossProcessTest,
                        ::testing::Bool());

IN_PROC_BROWSER_TEST_P(MimeHandlerViewCrossProcessTest, Embedded) {
  RunTest("test_embedded.html");
}

// The following tests will eventually converted into a parametric version which
// will run on both BrowserPlugin-based and cross-process-frame-based
// MimeHandlerView (https://crbug.com/659750).
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, PostMessage) {
  RunTest("test_postmessage.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Basic) {
  RunTest("testBasic.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Iframe) {
  RunTest("test_iframe.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Abort) {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // With the network service, abortStream isn't needed since we pass a Mojo
    // pipe to the renderer. If the plugin chooses to cancel the main request
    // (e.g. to make range requests instead), we are always guaranteed that the
    // Mojo pipe will be broken which will cancel the request. This is different
    // than without the network service, since stream URLs need to be explicitly
    // closed if they weren't yet opened to avoid leaks.
    // TODO(jam): once the network service is the only path, delete the
    // abortStream mimeHandlerPrivate method and supporting code.
    return;
  }
  RunTest("testAbort.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, NonAsciiHeaders) {
  RunTest("testNonAsciiHeaders.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, DataUrl) {
  const char* kDataUrlCsv = "data:text/csv;base64,Y29udGVudCB0byByZWFkCg==";
  RunTestWithUrl(GURL(kDataUrlCsv));
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlObject) {
  RunTest("test_embedded_data_url_object.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlEmbed) {
  RunTest("test_embedded_data_url_embed.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, EmbeddedDataUrlLong) {
  RunTest("test_embedded_data_url_long.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, ResizeBeforeAttach) {
  // Delay the creation of the guest's WebContents in order to delay the guest's
  // attachment to the embedder. This will allow us to resize the <object> tag
  // after the guest is created, but before it is attached in
  // "test_resize_before_attach.html".
  TestMimeHandlerViewGuest::DelayNextCreateWebContents(500);
  RunTest("test_resize_before_attach.html");

  // Wait for the guest to attach.
  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForSingleGuestCreated();
  TestMimeHandlerViewGuest* guest = static_cast<TestMimeHandlerViewGuest*>(
      MimeHandlerViewGuest::FromWebContents(guest_web_contents));
  guest->WaitForGuestAttached();

  // Ensure that the guest has the correct size after it has attached.
  auto guest_size = guest->size();
  CHECK_EQ(guest_size.width(), 500);
  CHECK_EQ(guest_size.height(), 400);
}

// Regression test for crbug.com/587709.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, SingleRequest) {
  GURL url(embedded_test_server()->GetURL("/testBasic.csv"));
  RunTest("testBasic.csv");
  EXPECT_EQ(1, basic_count());
}

// Test that a mime handler view can keep a background page alive.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BackgroundPage) {
  extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
  extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  RunTest("testBackgroundPage.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, TargetBlankAnchor) {
  RunTest("testTargetBlankAnchor.csv");
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WaitForLoadStop(browser()->tab_strip_model()->GetWebContentsAt(1));
  EXPECT_EQ(
      GURL("about:blank"),
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BeforeUnload_NoDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadNoDialog.csv"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure any beforeunload
  // toggle IPC has had time to reach the browser.
  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "");

  // Try to navigate away from the page. If the beforeunload listener is
  // triggered and a dialog is shown, this navigation will never complete,
  // causing the test to timeout and fail.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, BeforeUnload_ShowDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadShowDialog.csv"));
  auto* web_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure the beforeunload
  // toggle IPC has had time to reach the browser.
  ExecuteScriptAndGetValue(web_contents->GetMainFrame(), "");

  web_contents->GetController().LoadURL(GURL("about:blank"), {},
                                        ui::PAGE_TRANSITION_TYPED, "");

  app_modal::JavaScriptAppModalDialog* before_unload_dialog =
      ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(before_unload_dialog->is_before_unload_dialog());
  EXPECT_FALSE(before_unload_dialog->is_reload());
  before_unload_dialog->OnAccept(base::string16(), false);
}

// TODO(mcnee): These tests are BrowserPlugin specific. Once
// MimeHandlerViewGuest is no longer based on BrowserPlugin, remove these tests.
// (See https://crbug.com/533069 and https://crbug.com/659750). These category
// of tests are solely testing BrowserPlugin features.
class MimeHandlerViewBrowserPluginSpecificTest : public MimeHandlerViewTest {
 public:
  MimeHandlerViewBrowserPluginSpecificTest() {}

  ~MimeHandlerViewBrowserPluginSpecificTest() override {}

 protected:
  // None of these test create new tabs, so the embedder should be the first
  // tab.
  content::WebContents* GetEmbedderWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  DISALLOW_COPY_AND_ASSIGN(MimeHandlerViewBrowserPluginSpecificTest);
};

// This test verifies that when BrowserPlugin-based guest has touch handlers,
// the embedder knows about it.
IN_PROC_BROWSER_TEST_F(MimeHandlerViewBrowserPluginSpecificTest,
                       AcceptTouchEvents) {
  RunTest("testBasic.csv");
  content::RenderViewHost* embedder_rvh =
      GetEmbedderWebContents()->GetRenderViewHost();
  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);

  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  ASSERT_TRUE(ExecuteScript(
      guest_web_contents,
      "document.addEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder has touch handlers.
  while (!content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  ASSERT_TRUE(ExecuteScript(
      guest_web_contents,
      "document.removeEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder not longer has any touch handlers.
  while (content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}