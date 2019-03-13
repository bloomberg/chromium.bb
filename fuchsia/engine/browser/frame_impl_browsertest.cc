// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/common.h"
#include "fuchsia/engine/test/test_common.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

using testing::_;
using testing::AllOf;
using testing::Field;
using testing::InvokeWithoutArgs;
using testing::Mock;

// Use a shorter name for NavigationEvent, because it is
// referenced frequently in this file.
using NavigationDetails = chromium::web::NavigationEvent;

const char kPage1Path[] = "/title1.html";
const char kPage2Path[] = "/title2.html";
const char kDynamicTitlePath[] = "/dynamic_title.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";
const char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";
const char kTestServerRoot[] = FILE_PATH_LITERAL("fuchsia/engine/test/data");

MATCHER(IsSet, "Checks if an optional field is set.") {
  return !arg.is_null();
}

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class FrameImplTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  FrameImplTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath(kTestServerRoot));
  }

  ~FrameImplTest() = default;

  MOCK_METHOD1(OnServeHttpRequest,
               void(const net::test_server::HttpRequest& request));

 protected:
  // Creates a Frame with |navigation_observer_| attached.
  chromium::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_observer_);
  }

  // Navigates a |controller| to |url|, blocking until navigation is complete.
  void CheckLoadUrl(const std::string& url,
                    const std::string& expected_title,
                    chromium::web::LoadUrlParams load_url_params,
                    chromium::web::NavigationController* controller) {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, expected_title),
                    Field(&NavigationDetails::url, url))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(url, std::move(load_url_params));
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }

  testing::StrictMock<cr_fuchsia::MockNavigationObserver> navigation_observer_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FrameImplTest);
};

class WebContentsDeletionObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsDeletionObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  MOCK_METHOD1(RenderViewDeleted,
               void(content::RenderViewHost* render_view_host));
};

// Verifies that the browser will navigate and generate a navigation observer
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL,
               chromium::web::LoadUrlParams(), controller.get());
}

// TODO(crbug.com/931831): Remove this test once the transition is complete.
IN_PROC_BROWSER_TEST_F(FrameImplTest, DeprecatedNavigateFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(testing::AllOf(
                  Field(&NavigationDetails::title, url::kAboutBlankURL),
                  Field(&NavigationDetails::url, url::kAboutBlankURL))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  controller->LoadUrl2(url::kAboutBlankURL, chromium::web::LoadUrlParams2());
  run_loop.Run();
  Mock::VerifyAndClearExpectations(this);
  navigation_observer_.Acknowledge();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  CheckLoadUrl(kDataUrl, kDataUrl, chromium::web::LoadUrlParams(),
               controller.get());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  chromium::web::FramePtr frame = CreateFrame();

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  WebContentsDeletionObserver deletion_observer(
      frame_impl->web_contents_for_test());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  controller->LoadUrl(url::kAboutBlankURL, chromium::web::LoadUrlParams());

  frame.Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrameWithView) {
  chromium::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  zx::eventpair import_token;
  fuchsia::ui::gfx::ExportToken export_token;
  ASSERT_EQ(zx::eventpair::create(0, &import_token, &export_token.value),
            ZX_OK);

  frame->CreateView(std::move(export_token));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  CheckLoadUrl(title1.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               controller.get());
  CheckLoadUrl(title2.spec(), kPage2Title, chromium::web::LoadUrlParams(),
               controller.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the top of the navigation entry list; this should be a no-op.
  controller->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoForward();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // At the end of the navigation entry list; this should be a no-op.
  controller->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ReloadFrame) {
  chromium::web::FramePtr frame = CreateFrame();
  chromium::web::NavigationControllerPtr navigation_controller;
  frame->GetNavigationController(navigation_controller.NewRequest());

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_));
  CheckLoadUrl(url.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               navigation_controller.get());

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Reload with NO_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::NO_CACHE);
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
    navigation_observer_.Acknowledge();
  }
  // Reload with PARTIAL_CACHE.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnServeHttpRequest(_));
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, url))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_controller->Reload(chromium::web::ReloadType::PARTIAL_CACHE);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GetVisibleEntry) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that a Frame returns a null NavigationEntry prior to receiving any
  // LoadUrl() calls.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_EQ(nullptr, details.get());
          run_loop.Quit();
        });
    run_loop.Run();
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Navigate to a page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify that GetVisibleEntry() reflects the new Frame navigation state.
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate to another page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage2Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title2](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title2.spec());
          EXPECT_EQ(details->title, kPage2Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  // Navigate back to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(testing::InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));
    controller->GoBack();
    run_loop.Run();
    navigation_observer_.Acknowledge();
  }

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    controller->GetVisibleEntry(
        [&run_loop,
         &title1](std::unique_ptr<chromium::web::NavigationEntry> details) {
          EXPECT_TRUE(details);
          EXPECT_EQ(details->url, title1.spec());
          EXPECT_EQ(details->title, kPage1Title);
          run_loop.Quit();
        });
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  chromium::web::FramePtr frame;
  context()->CreateFrame(frame.NewRequest());
  base::RunLoop().RunUntilIdle();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
  }
}

// Test JS injection by using Javascript to trigger document navigation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptImmediate) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(title1.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               controller.get());
  std::vector<std::string> origins = {title1.GetOrigin().spec()};

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("window.location.href = \"" +
                                      title2.spec() + "\";"),
      chromium::web::ExecuteMode::IMMEDIATE_ONCE,
      [](bool success) { EXPECT_TRUE(success); });

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(
                  testing::AllOf(Field(&NavigationDetails::title, kPage2Title),
                                 Field(&NavigationDetails::url, IsSet()))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptOnLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {url.GetOrigin().spec()};

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", chromium::web::LoadUrlParams(),
               controller.get());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptOnLoadVmoDestroyed) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {url.GetOrigin().spec()};

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", chromium::web::LoadUrlParams(),
               controller.get());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavascriptOnLoadWrongOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {"http://example.com"};

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Expect that the original HTML title is used, because we didn't inject a
  // script with a replacement title.
  CheckLoadUrl(url.spec(), "Welcome to Stan the Offline Dino's Homepage",
               chromium::web::LoadUrlParams(), controller.get());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptOnLoadWildcardOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {"*"};

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  // Test script injection for the origin 127.0.0.1.
  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", chromium::web::LoadUrlParams(),
               controller.get());

  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL,
               chromium::web::LoadUrlParams(), controller.get());

  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard.
  GURL alt_url = embedded_test_server()->GetURL("localhost", kDynamicTitlePath);
  CheckLoadUrl(alt_url.spec(), "hello", chromium::web::LoadUrlParams(),
               controller.get());
}

// Test that consecutive scripts are executed in order by computing a cumulative
// result.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteMultipleJavaScriptsOnLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {url.GetOrigin().spec()};
  frame->ExecuteJavaScript(
      origins, cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });
  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title += ' there';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello there", chromium::web::LoadUrlParams(),
               controller.get());
}

// Test that we can inject scripts before and after RenderFrame creation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteOnLoadEarlyAndLateRegistrations) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  chromium::web::FramePtr frame = CreateFrame();

  std::vector<std::string> origins = {url.GetOrigin().spec()};

  frame->ExecuteJavaScript(
      origins, cr_fuchsia::MemBufferFromString("stashed_title = 'hello';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), "hello", chromium::web::LoadUrlParams(),
               controller.get());

  frame->ExecuteJavaScript(
      std::move(origins),
      cr_fuchsia::MemBufferFromString("stashed_title += ' there';"),
      chromium::web::ExecuteMode::ON_PAGE_LOAD,
      [](bool success) { EXPECT_TRUE(success); });

  // Navigate away to clean the slate.
  CheckLoadUrl(url::kAboutBlankURL, url::kAboutBlankURL,
               chromium::web::LoadUrlParams(), controller.get());

  // Navigate back and see if both scripts are working.
  CheckLoadUrl(url.spec(), "hello there", chromium::web::LoadUrlParams(),
               controller.get());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptBadEncoding) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(url.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               controller.get());

  base::RunLoop run_loop;

  // 0xFE is an illegal UTF-8 byte; it should cause UTF-8 conversion to fail.
  std::vector<std::string> origins = {url.host()};
  frame->ExecuteJavaScript(
      std::move(origins), cr_fuchsia::MemBufferFromString("true;\xfe"),
      chromium::web::ExecuteMode::IMMEDIATE_ONCE, [&run_loop](bool success) {
        EXPECT_FALSE(success);
        run_loop.Quit();
      });
  run_loop.Run();
}

// Verifies that a Frame will handle navigation observer disconnection events
// gracefully.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationObserverDisconnected) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1));
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
  }

  // Disconnect the observer & spin the runloop to propagate the disconnection
  // event over IPC.
  navigation_observer_bindings().CloseAll();
  base::RunLoop().RunUntilIdle();

  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DelayedNavigationEventAck) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Expect an navigation event here, but deliberately postpone acknowledgement
  // until the end of the test.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since we have blocked NavigationEventObserver's flow, we must observe the
  // WebContents events directly via a test-only seam.
  navigation_observer_.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  // Navigate to a second page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title2))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title2.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Navigate to the first page.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_, DidFinishLoad(_, title1))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(title1.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(navigation_observer_,
                MockableOnNavigationStateChanged(testing::AllOf(
                    Field(&NavigationDetails::title, kPage1Title),
                    Field(&NavigationDetails::url, IsSet()))))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    navigation_observer_.Acknowledge();
    run_loop.Run();
  }
}

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());

  // Use a request handler that will accept the connection and stall
  // indefinitely.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));

  WebContentsObserverForStop observer;
  observer.Observe(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(_))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(hung_url.spec(), chromium::web::LoadUrlParams());
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_TRUE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, NavigationStopped())
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Stop();
    run_loop.Run();
    Mock::VerifyAndClearExpectations(this);
  }

  EXPECT_FALSE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessage) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(
      embedded_test_server()->GetURL("/window_post_message.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "postmessage",
               chromium::web::LoadUrlParams(), controller.get());

  chromium::web::WebMessage message;
  message.data = cr_fuchsia::MemBufferFromString(kPage1Path);
  cr_fuchsia::ResultReceiver<bool> post_result;
  frame->PostMessage(
      std::move(message), post_message_url.GetOrigin().spec(),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(
                  testing::AllOf(Field(&NavigationDetails::title, kPage1Title),
                                 Field(&NavigationDetails::url, IsSet()))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  run_loop.Run();
  EXPECT_TRUE(*post_result);
}

// Send a MessagePort to the content, then perform bidirectional messaging
// through the port.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessagePassMessagePort) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport",
               chromium::web::LoadUrlParams(), controller.get());

  chromium::web::MessagePortPtr message_port;
  chromium::web::WebMessage msg;
  {
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = cr_fuchsia::MemBufferFromString("hi");
    cr_fuchsia::ResultReceiver<bool> post_result;
    frame->PostMessage(
        std::move(msg), post_message_url.GetOrigin().spec(),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("got_port", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
  }

  {
    msg.data = cr_fuchsia::MemBufferFromString("ping");
    cr_fuchsia::ResultReceiver<bool> post_result;
    message_port->PostMessage(
        std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("ack ping", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
    EXPECT_TRUE(*post_result);
  }
}

// Send a MessagePort to the content, then perform bidirectional messaging
// over its channel.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageMessagePortDisconnected) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport",
               chromium::web::LoadUrlParams(), controller.get());

  chromium::web::MessagePortPtr message_port;
  chromium::web::WebMessage msg;
  {
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = cr_fuchsia::MemBufferFromString("hi");
    cr_fuchsia::ResultReceiver<bool> post_result;
    frame->PostMessage(
        std::move(msg), post_message_url.GetOrigin().spec(),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("got_port", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
    EXPECT_TRUE(*post_result);
  }

  // Navigating off-page should tear down the Mojo channel, thereby causing the
  // MessagePortImpl to self-destruct and tear down its FIDL channel.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler(
        [&run_loop](zx_status_t) { run_loop.Quit(); });
    controller->LoadUrl(url::kAboutBlankURL, chromium::web::LoadUrlParams());
    run_loop.Run();
  }
}

// Send a MessagePort to the content, and through that channel, receive a
// different MessagePort that was created by the content. Verify the second
// channel's liveness by sending a ping to it.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageUseContentProvidedPort) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport",
               chromium::web::LoadUrlParams(), controller.get());

  chromium::web::MessagePortPtr incoming_message_port;
  chromium::web::WebMessage msg;
  {
    chromium::web::MessagePortPtr message_port;
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(message_port.NewRequest());
    msg.data = cr_fuchsia::MemBufferFromString("hi");
    cr_fuchsia::ResultReceiver<bool> post_result;
    frame->PostMessage(
        std::move(msg), "*",
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("got_port", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
    incoming_message_port = receiver->incoming_transfer->message_port().Bind();
    EXPECT_TRUE(*post_result);
  }

  // Get the content to send three 'ack ping' messages, which will accumulate in
  // the MessagePortImpl buffer.
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<bool> post_result(run_loop.QuitClosure());
    msg.data = cr_fuchsia::MemBufferFromString("ping");
    incoming_message_port->PostMessage(
        std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_TRUE(*post_result);
  }

  // Receive another acknowledgement from content on a side channel to ensure
  // that all the "ack pings" are ready to be consumed.
  {
    chromium::web::MessagePortPtr ack_message_port;
    chromium::web::WebMessage msg;
    msg.outgoing_transfer =
        std::make_unique<chromium::web::OutgoingTransferable>();
    msg.outgoing_transfer->set_message_port(ack_message_port.NewRequest());
    msg.data = cr_fuchsia::MemBufferFromString("hi");

    // Quit the runloop only after we've received a WebMessage AND a PostMessage
    // result.
    cr_fuchsia::ResultReceiver<bool> post_result;
    frame->PostMessage(
        std::move(msg), "*",
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    ack_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("got_port", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
    EXPECT_TRUE(*post_result);
  }

  // Pull the three 'ack ping's from the buffer.
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
        run_loop.QuitClosure());
    incoming_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_EQ("ack ping", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageBadOriginDropped) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  CheckLoadUrl(post_message_url.spec(), "messageport",
               chromium::web::LoadUrlParams(), controller.get());

  chromium::web::MessagePortPtr bad_origin_incoming_message_port;
  chromium::web::WebMessage msg;

  // PostMessage() to invalid origins should be ignored. We pass in a
  // MessagePort but nothing should happen to it.
  chromium::web::MessagePortPtr unused_message_port;
  msg.outgoing_transfer =
      std::make_unique<chromium::web::OutgoingTransferable>();
  msg.outgoing_transfer->set_message_port(unused_message_port.NewRequest());
  msg.data = cr_fuchsia::MemBufferFromString("bad origin, bad!");
  cr_fuchsia::ResultReceiver<bool> unused_post_result;
  frame->PostMessage(std::move(msg), "https://example.com",
                     cr_fuchsia::CallbackToFitFunction(
                         unused_post_result.GetReceiveCallback()));
  cr_fuchsia::ResultReceiver<chromium::web::WebMessage> unused_message_read;
  bad_origin_incoming_message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(
          unused_message_read.GetReceiveCallback()));

  // PostMessage() with a valid origin should succeed.
  // Verify it by looking for an ack message on the MessagePort we passed in.
  // Since message events are handled in order, observing the result of this
  // operation will verify whether the previous PostMessage() was received but
  // discarded.
  chromium::web::MessagePortPtr incoming_message_port;
  chromium::web::MessagePortPtr message_port;
  msg.outgoing_transfer =
      std::make_unique<chromium::web::OutgoingTransferable>();
  msg.outgoing_transfer->set_message_port(message_port.NewRequest());
  msg.data = cr_fuchsia::MemBufferFromString("good origin");
  cr_fuchsia::ResultReceiver<bool> post_result;
  frame->PostMessage(
      std::move(msg), "*",
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<chromium::web::WebMessage> receiver(
      run_loop.QuitClosure());
  message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
  run_loop.Run();
  EXPECT_EQ("got_port", cr_fuchsia::StringFromMemBufferOrDie(receiver->data));
  incoming_message_port = receiver->incoming_transfer->message_port().Bind();
  EXPECT_TRUE(*post_result);

  // Verify that the first PostMessage() call wasn't handled.
  EXPECT_FALSE(unused_message_read.has_value());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, RecreateView) {
  chromium::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Process the Frame creation request, and verify we can get the FrameImpl.
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  ASSERT_TRUE(frame_impl);
  EXPECT_FALSE(frame_impl->has_view_for_test());

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that the Frame can navigate, prior to the View being created.
  const GURL page1_url(embedded_test_server()->GetURL(kPage1Path));
  CheckLoadUrl(page1_url.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               controller.get());

  // Request a View from the Frame, and pump the loop to process the request.
  zx::eventpair owner_token, frame_token;
  ASSERT_EQ(zx::eventpair::create(0, &owner_token, &frame_token), ZX_OK);
  frame->CreateView2(std::move(frame_token), nullptr, nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating to Page #2.
  const GURL page2_url(embedded_test_server()->GetURL(kPage2Path));
  CheckLoadUrl(page2_url.spec(), kPage2Title, chromium::web::LoadUrlParams(),
               controller.get());

  // Create new View tokens and request a new view.
  zx::eventpair owner_token2, frame_token2;
  ASSERT_EQ(zx::eventpair::create(0, &owner_token2, &frame_token2), ZX_OK);
  frame->CreateView2(std::move(frame_token), nullptr, nullptr);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating back to Page #1.
  CheckLoadUrl(page1_url.spec(), kPage1Title, chromium::web::LoadUrlParams(),
               controller.get());
}

class RequestMonitoringFrameImplBrowserTest : public FrameImplTest {
 public:
  RequestMonitoringFrameImplBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    // Accumulate all http requests made to |embedded_test_server| into
    // |accumulated_requests_| container.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &RequestMonitoringFrameImplBrowserTest::MonitorRequestOnIoThread,
        base::Unretained(this), base::SequencedTaskRunnerHandle::Get()));

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDown() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  std::map<GURL, net::test_server::HttpRequest> accumulated_requests_;

 private:
  void MonitorRequestOnIoThread(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const net::test_server::HttpRequest& request) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RequestMonitoringFrameImplBrowserTest::MonitorRequestOnMainThread,
            base::Unretained(this), request));
  }

  void MonitorRequestOnMainThread(
      const net::test_server::HttpRequest& request) {
    accumulated_requests_[request.GetURL()] = request;
  }

  DISALLOW_COPY_AND_ASSIGN(RequestMonitoringFrameImplBrowserTest);
};

std::vector<uint8_t> StringToUnsignedVector(base::StringPiece str) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(str.data());
  return std::vector<uint8_t>(raw_data, raw_data + str.length());
}

IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest, ExtraHeaders) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::LoadUrlParams load_url_params;
  load_url_params.set_headers({StringToUnsignedVector("X-ExtraHeaders: 1"),
                               StringToUnsignedVector("X-2ExtraHeaders: 2")});

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  CheckLoadUrl(page_url.spec(), kPage1Title, std::move(load_url_params),
               controller.get());
  base::RunLoop().RunUntilIdle();

  // At this point, the page should be loaded, the server should have received
  // the request and the request should be in the map.
  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-ExtraHeaders")));
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-2ExtraHeaders")));
}

// TODO(crbug.com/931831): Remove this test once the transition is complete.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       DeprecatedExtraHeaders) {
  chromium::web::FramePtr frame = CreateFrame();

  chromium::web::LoadUrlParams2 load_url_params;
  load_url_params.set_headers({StringToUnsignedVector("X-ExtraHeaders: 1"),
                               StringToUnsignedVector("X-2ExtraHeaders: 2")});

  chromium::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));

  base::RunLoop run_loop;
  EXPECT_CALL(navigation_observer_,
              MockableOnNavigationStateChanged(testing::AllOf(
                  Field(&NavigationDetails::title, kPage1Title),
                  Field(&NavigationDetails::url, page_url.spec()))))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  controller->LoadUrl2(page_url.spec(), std::move(load_url_params));
  run_loop.Run();
  Mock::VerifyAndClearExpectations(this);
  navigation_observer_.Acknowledge();

  base::RunLoop().RunUntilIdle();

  // At this point, the page should be loaded, the server should have received
  // the request and the request should be in the map.
  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-ExtraHeaders")));
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-2ExtraHeaders")));
}
