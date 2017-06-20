// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/browser.h"
#include "headless/public/devtools/domains/dom.h"
#include "headless/public/devtools/domains/dom_snapshot.h"
#include "headless/public/devtools/domains/emulation.h"
#include "headless/public/devtools/domains/inspector.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/devtools/domains/target.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/test_protocol_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#define EXPECT_SIZE_EQ(expected, actual)               \
  do {                                                 \
    EXPECT_EQ((expected).width(), (actual).width());   \
    EXPECT_EQ((expected).height(), (actual).height()); \
  } while (false)

using testing::ElementsAre;

namespace headless {

namespace {

std::vector<HeadlessWebContents*> GetAllWebContents(HeadlessBrowser* browser) {
  std::vector<HeadlessWebContents*> result;

  for (HeadlessBrowserContext* browser_context :
       browser->GetAllBrowserContexts()) {
    std::vector<HeadlessWebContents*> web_contents =
        browser_context->GetAllWebContents();
    result.insert(result.end(), web_contents.begin(), web_contents.end());
  }

  return result;
}

}  // namespace

class HeadlessDevToolsClientNavigationTest
    : public HeadlessAsyncDevTooledBrowserTest,
      page::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    std::unique_ptr<page::NavigateParams> params =
        page::NavigateParams::Builder()
            .SetUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .Build();
    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    base::RunLoop run_loop;
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    run_loop.Run();
    devtools_client_->GetPage()->Navigate(std::move(params));
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->GetExperimental()->RemoveObserver(this);
    FinishAsynchronousTest();
  }

  // Check that events with no parameters still get a parameters object.
  void OnFrameResized(const page::FrameResizedParams& params) override {}
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientNavigationTest);

class HeadlessDevToolsClientWindowManagementTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void SetWindowBounds(
      const gfx::Rect& rect,
      base::Callback<void(std::unique_ptr<browser::SetWindowBoundsResult>)>
          callback) {
    std::unique_ptr<headless::browser::Bounds> bounds =
        headless::browser::Bounds::Builder()
            .SetLeft(rect.x())
            .SetTop(rect.y())
            .SetWidth(rect.width())
            .SetHeight(rect.height())
            .SetWindowState(browser::WindowState::NORMAL)
            .Build();
    int window_id = HeadlessWebContentsImpl::From(web_contents_)->window_id();
    std::unique_ptr<browser::SetWindowBoundsParams> params =
        browser::SetWindowBoundsParams::Builder()
            .SetWindowId(window_id)
            .SetBounds(std::move(bounds))
            .Build();
    browser_devtools_client_->GetBrowser()->GetExperimental()->SetWindowBounds(
        std::move(params), callback);
  }

  void SetWindowState(
      const browser::WindowState state,
      base::Callback<void(std::unique_ptr<browser::SetWindowBoundsResult>)>
          callback) {
    std::unique_ptr<headless::browser::Bounds> bounds =
        headless::browser::Bounds::Builder().SetWindowState(state).Build();
    int window_id = HeadlessWebContentsImpl::From(web_contents_)->window_id();
    std::unique_ptr<browser::SetWindowBoundsParams> params =
        browser::SetWindowBoundsParams::Builder()
            .SetWindowId(window_id)
            .SetBounds(std::move(bounds))
            .Build();
    browser_devtools_client_->GetBrowser()->GetExperimental()->SetWindowBounds(
        std::move(params), callback);
  }

  void GetWindowBounds(
      base::Callback<void(std::unique_ptr<browser::GetWindowBoundsResult>)>
          callback) {
    int window_id = HeadlessWebContentsImpl::From(web_contents_)->window_id();
    std::unique_ptr<browser::GetWindowBoundsParams> params =
        browser::GetWindowBoundsParams::Builder()
            .SetWindowId(window_id)
            .Build();

    browser_devtools_client_->GetBrowser()->GetExperimental()->GetWindowBounds(
        std::move(params), callback);
  }

  void CheckWindowBounds(
      const gfx::Rect& bounds,
      const browser::WindowState state,
      std::unique_ptr<browser::GetWindowBoundsResult> result) {
    const headless::browser::Bounds* actual_bounds = result->GetBounds();
// Mac does not support repositioning, as we don't show any actual window.
#if !defined(OS_MACOSX)
    EXPECT_EQ(bounds.x(), actual_bounds->GetLeft());
    EXPECT_EQ(bounds.y(), actual_bounds->GetTop());
#endif  // !defined(OS_MACOSX)
    EXPECT_EQ(bounds.width(), actual_bounds->GetWidth());
    EXPECT_EQ(bounds.height(), actual_bounds->GetHeight());
    EXPECT_EQ(state, actual_bounds->GetWindowState());
  }
};

class HeadlessDevToolsClientChangeWindowBoundsTest
    : public HeadlessDevToolsClientWindowManagementTest {
  void RunDevTooledTest() override {
    SetWindowBounds(
        gfx::Rect(100, 200, 300, 400),
        base::Bind(
            &HeadlessDevToolsClientChangeWindowBoundsTest::OnSetWindowBounds,
            base::Unretained(this)));
  }

  void OnSetWindowBounds(
      std::unique_ptr<browser::SetWindowBoundsResult> result) {
    GetWindowBounds(base::Bind(
        &HeadlessDevToolsClientChangeWindowBoundsTest::OnGetWindowBounds,
        base::Unretained(this)));
  }

  void OnGetWindowBounds(
      std::unique_ptr<browser::GetWindowBoundsResult> result) {
    CheckWindowBounds(gfx::Rect(100, 200, 300, 400),
                      browser::WindowState::NORMAL, std::move(result));
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientChangeWindowBoundsTest);

class HeadlessDevToolsClientChangeWindowStateTest
    : public HeadlessDevToolsClientWindowManagementTest {
 public:
  explicit HeadlessDevToolsClientChangeWindowStateTest(
      browser::WindowState state)
      : state_(state){};

  void RunDevTooledTest() override {
    SetWindowState(
        state_,
        base::Bind(
            &HeadlessDevToolsClientChangeWindowStateTest::OnSetWindowState,
            base::Unretained(this)));
  }

  void OnSetWindowState(
      std::unique_ptr<browser::SetWindowBoundsResult> result) {
    GetWindowBounds(base::Bind(
        &HeadlessDevToolsClientChangeWindowStateTest::OnGetWindowState,
        base::Unretained(this)));
  }

  void OnGetWindowState(
      std::unique_ptr<browser::GetWindowBoundsResult> result) {
    HeadlessBrowser::Options::Builder builder;
    const HeadlessBrowser::Options kDefaultOptions = builder.Build();
    CheckWindowBounds(gfx::Rect(kDefaultOptions.window_size), state_,
                      std::move(result));
    FinishAsynchronousTest();
  }

 protected:
  browser::WindowState state_;
};

class HeadlessDevToolsClientMinimizeWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientMinimizeWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest(
            browser::WindowState::MINIMIZED){};
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientMinimizeWindowTest);

class HeadlessDevToolsClientMaximizeWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientMaximizeWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest(
            browser::WindowState::MAXIMIZED){};
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientMaximizeWindowTest);

class HeadlessDevToolsClientFullscreenWindowTest
    : public HeadlessDevToolsClientChangeWindowStateTest {
 public:
  HeadlessDevToolsClientFullscreenWindowTest()
      : HeadlessDevToolsClientChangeWindowStateTest(
            browser::WindowState::FULLSCREEN){};
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientFullscreenWindowTest);

class HeadlessDevToolsClientEvalTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    std::unique_ptr<runtime::EvaluateParams> params =
        runtime::EvaluateParams::Builder().SetExpression("1 + 2").Build();
    devtools_client_->GetRuntime()->Evaluate(
        std::move(params),
        base::Bind(&HeadlessDevToolsClientEvalTest::OnFirstResult,
                   base::Unretained(this)));
    // Test the convenience overload which only takes the required command
    // parameters.
    devtools_client_->GetRuntime()->Evaluate(
        "24 * 7", base::Bind(&HeadlessDevToolsClientEvalTest::OnSecondResult,
                             base::Unretained(this)));
  }

  void OnFirstResult(std::unique_ptr<runtime::EvaluateResult> result) {
    int value;
    EXPECT_TRUE(result->GetResult()->HasValue());
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsInteger(&value));
    EXPECT_EQ(3, value);
  }

  void OnSecondResult(std::unique_ptr<runtime::EvaluateResult> result) {
    int value;
    EXPECT_TRUE(result->GetResult()->HasValue());
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsInteger(&value));
    EXPECT_EQ(168, value);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientEvalTest);

class HeadlessDevToolsClientCallbackTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  HeadlessDevToolsClientCallbackTest() : first_result_received_(false) {}

  void RunDevTooledTest() override {
    // Null callback without parameters.
    devtools_client_->GetPage()->Enable();
    // Null callback with parameters.
    devtools_client_->GetRuntime()->Evaluate("true");
    // Non-null callback without parameters.
    devtools_client_->GetPage()->Disable(
        base::Bind(&HeadlessDevToolsClientCallbackTest::OnFirstResult,
                   base::Unretained(this)));
    // Non-null callback with parameters.
    devtools_client_->GetRuntime()->Evaluate(
        "true", base::Bind(&HeadlessDevToolsClientCallbackTest::OnSecondResult,
                           base::Unretained(this)));
  }

  void OnFirstResult() {
    EXPECT_FALSE(first_result_received_);
    first_result_received_ = true;
  }

  void OnSecondResult(std::unique_ptr<runtime::EvaluateResult> result) {
    EXPECT_TRUE(first_result_received_);
    FinishAsynchronousTest();
  }

 private:
  bool first_result_received_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientCallbackTest);

class HeadlessDevToolsClientObserverTest
    : public HeadlessAsyncDevTooledBrowserTest,
      network::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();

    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnRequestWillBeSent(
      const network::RequestWillBeSentParams& params) override {
    EXPECT_EQ("GET", params.GetRequest()->GetMethod());
    EXPECT_EQ(embedded_test_server()->GetURL("/hello.html").spec(),
              params.GetRequest()->GetUrl());
  }

  void OnResponseReceived(
      const network::ResponseReceivedParams& params) override {
    EXPECT_EQ(200, params.GetResponse()->GetStatus());
    EXPECT_EQ("OK", params.GetResponse()->GetStatusText());
    std::string content_type;
    EXPECT_TRUE(params.GetResponse()->GetHeaders()->GetString("Content-Type",
                                                              &content_type));
    EXPECT_EQ("text/html", content_type);

    devtools_client_->GetNetwork()->Disable();
    devtools_client_->GetNetwork()->RemoveObserver(this);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientObserverTest);

class HeadlessDevToolsClientExperimentalTest
    : public HeadlessAsyncDevTooledBrowserTest,
      page::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    // Check that experimental commands require parameter objects.
    devtools_client_->GetRuntime()
        ->GetExperimental()
        ->SetCustomObjectFormatterEnabled(
            runtime::SetCustomObjectFormatterEnabledParams::Builder()
                .SetEnabled(false)
                .Build());

    // Check that a previously experimental command which takes no parameters
    // still works by giving it a parameter object.
    devtools_client_->GetRuntime()->GetExperimental()->RunIfWaitingForDebugger(
        runtime::RunIfWaitingForDebuggerParams::Builder().Build());

    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnFrameStoppedLoading(
      const page::FrameStoppedLoadingParams& params) override {
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->GetExperimental()->RemoveObserver(this);

    // Check that a non-experimental command which has no return value can be
    // called with a void() callback.
    devtools_client_->GetPage()->Reload(
        page::ReloadParams::Builder().Build(),
        base::Bind(&HeadlessDevToolsClientExperimentalTest::OnReloadStarted,
                   base::Unretained(this)));
  }

  void OnReloadStarted() { FinishAsynchronousTest(); }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientExperimentalTest);

class TargetDomainCreateAndDeletePageTest
    : public HeadlessAsyncDevTooledBrowserTest {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    EXPECT_EQ(1u, GetAllWebContents(browser()).size());

    devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetWidth(1)
            .SetHeight(1)
            .Build(),
        base::Bind(&TargetDomainCreateAndDeletePageTest::OnCreateTargetResult,
                   base::Unretained(this)));
  }

  void OnCreateTargetResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    EXPECT_EQ(2u, GetAllWebContents(browser()).size());

    HeadlessWebContentsImpl* contents = HeadlessWebContentsImpl::From(
        browser()->GetWebContentsForDevToolsAgentHostId(result->GetTargetId()));
    EXPECT_SIZE_EQ(gfx::Size(1, 1), contents->web_contents()
                                        ->GetRenderWidgetHostView()
                                        ->GetViewBounds()
                                        .size());

    devtools_client_->GetTarget()->GetExperimental()->CloseTarget(
        target::CloseTargetParams::Builder()
            .SetTargetId(result->GetTargetId())
            .Build(),
        base::Bind(&TargetDomainCreateAndDeletePageTest::OnCloseTargetResult,
                   base::Unretained(this)));
  }

  void OnCloseTargetResult(std::unique_ptr<target::CloseTargetResult> result) {
    EXPECT_TRUE(result->GetSuccess());
    EXPECT_EQ(1u, GetAllWebContents(browser()).size());
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(TargetDomainCreateAndDeletePageTest);

class TargetDomainCreateAndDeleteBrowserContextTest
    : public HeadlessAsyncDevTooledBrowserTest {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    EXPECT_EQ(1u, GetAllWebContents(browser()).size());

    devtools_client_->GetTarget()->GetExperimental()->CreateBrowserContext(
        target::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&TargetDomainCreateAndDeleteBrowserContextTest::
                       OnCreateContextResult,
                   base::Unretained(this)));
  }

  void OnCreateContextResult(
      std::unique_ptr<target::CreateBrowserContextResult> result) {
    browser_context_id_ = result->GetBrowserContextId();

    devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetBrowserContextId(result->GetBrowserContextId())
            .SetWidth(1)
            .SetHeight(1)
            .Build(),
        base::Bind(&TargetDomainCreateAndDeleteBrowserContextTest::
                       OnCreateTargetResult,
                   base::Unretained(this)));
  }

  void OnCreateTargetResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    EXPECT_EQ(2u, GetAllWebContents(browser()).size());

    devtools_client_->GetTarget()->GetExperimental()->CloseTarget(
        target::CloseTargetParams::Builder()
            .SetTargetId(result->GetTargetId())
            .Build(),
        base::Bind(&TargetDomainCreateAndDeleteBrowserContextTest::
                       OnCloseTargetResult,
                   base::Unretained(this)));
  }

  void OnCloseTargetResult(std::unique_ptr<target::CloseTargetResult> result) {
    EXPECT_EQ(1u, GetAllWebContents(browser()).size());
    EXPECT_TRUE(result->GetSuccess());

    devtools_client_->GetTarget()->GetExperimental()->DisposeBrowserContext(
        target::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(browser_context_id_)
            .Build(),
        base::Bind(&TargetDomainCreateAndDeleteBrowserContextTest::
                       OnDisposeBrowserContextResult,
                   base::Unretained(this)));
  }

  void OnDisposeBrowserContextResult(
      std::unique_ptr<target::DisposeBrowserContextResult> result) {
    EXPECT_TRUE(result->GetSuccess());
    FinishAsynchronousTest();
  }

 private:
  std::string browser_context_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(TargetDomainCreateAndDeleteBrowserContextTest);

class TargetDomainDisposeContextFailsIfInUse
    : public HeadlessAsyncDevTooledBrowserTest {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    EXPECT_EQ(1u, GetAllWebContents(browser()).size());
    devtools_client_->GetTarget()->GetExperimental()->CreateBrowserContext(
        target::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&TargetDomainDisposeContextFailsIfInUse::OnContextCreated,
                   base::Unretained(this)));
  }

  void OnContextCreated(
      std::unique_ptr<target::CreateBrowserContextResult> result) {
    context_id_ = result->GetBrowserContextId();

    devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(
            &TargetDomainDisposeContextFailsIfInUse::OnCreateTargetResult,
            base::Unretained(this)));
  }

  void OnCreateTargetResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    page_id_ = result->GetTargetId();

    devtools_client_->GetTarget()->GetExperimental()->DisposeBrowserContext(
        target::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(&TargetDomainDisposeContextFailsIfInUse::
                       OnDisposeBrowserContextResult,
                   base::Unretained(this)));
  }

  void OnDisposeBrowserContextResult(
      std::unique_ptr<target::DisposeBrowserContextResult> result) {
    EXPECT_FALSE(result->GetSuccess());

    // Close the page and try again.
    devtools_client_->GetTarget()->GetExperimental()->CloseTarget(
        target::CloseTargetParams::Builder().SetTargetId(page_id_).Build(),
        base::Bind(
            &TargetDomainDisposeContextFailsIfInUse::OnCloseTargetResult,
            base::Unretained(this)));
  }

  void OnCloseTargetResult(std::unique_ptr<target::CloseTargetResult> result) {
    EXPECT_TRUE(result->GetSuccess());

    devtools_client_->GetTarget()->GetExperimental()->DisposeBrowserContext(
        target::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(&TargetDomainDisposeContextFailsIfInUse::
                       OnDisposeBrowserContextResult2,
                   base::Unretained(this)));
  }

  void OnDisposeBrowserContextResult2(
      std::unique_ptr<target::DisposeBrowserContextResult> result) {
    EXPECT_TRUE(result->GetSuccess());
    FinishAsynchronousTest();
  }

 private:
  std::string context_id_;
  std::string page_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(TargetDomainDisposeContextFailsIfInUse);

class TargetDomainCreateTwoContexts : public HeadlessAsyncDevTooledBrowserTest,
                                      public target::ExperimentalObserver,
                                      public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    base::RunLoop run_loop;
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();

    devtools_client_->GetTarget()->GetExperimental()->AddObserver(this);
    devtools_client_->GetTarget()->GetExperimental()->CreateBrowserContext(
        target::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnContextOneCreated,
                   base::Unretained(this)));

    devtools_client_->GetTarget()->GetExperimental()->CreateBrowserContext(
        target::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnContextTwoCreated,
                   base::Unretained(this)));
  }

  void OnContextOneCreated(
      std::unique_ptr<target::CreateBrowserContextResult> result) {
    context_id_one_ = result->GetBrowserContextId();
    MaybeCreatePages();
  }

  void OnContextTwoCreated(
      std::unique_ptr<target::CreateBrowserContextResult> result) {
    context_id_two_ = result->GetBrowserContextId();
    MaybeCreatePages();
  }

  void MaybeCreatePages() {
    if (context_id_one_.empty() || context_id_two_.empty())
      return;

    devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl("about://blank")
            .SetBrowserContextId(context_id_one_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnCreateTargetOneResult,
                   base::Unretained(this)));

    devtools_client_->GetTarget()->GetExperimental()->CreateTarget(
        target::CreateTargetParams::Builder()
            .SetUrl("about://blank")
            .SetBrowserContextId(context_id_two_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnCreateTargetTwoResult,
                   base::Unretained(this)));
  }

  void OnCreateTargetOneResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    page_id_one_ = result->GetTargetId();
    MaybeTestIsolation();
  }

  void OnCreateTargetTwoResult(
      std::unique_ptr<target::CreateTargetResult> result) {
    page_id_two_ = result->GetTargetId();
    MaybeTestIsolation();
  }

  void MaybeTestIsolation() {
    if (page_id_one_.empty() || page_id_two_.empty())
      return;

    devtools_client_->GetTarget()->GetExperimental()->AttachToTarget(
        target::AttachToTargetParams::Builder()
            .SetTargetId(page_id_one_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnAttachedToTargetOne,
                   base::Unretained(this)));

    devtools_client_->GetTarget()->GetExperimental()->AttachToTarget(
        target::AttachToTargetParams::Builder()
            .SetTargetId(page_id_two_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnAttachedToTargetTwo,
                   base::Unretained(this)));
  }

  void OnAttachedToTargetOne(
      std::unique_ptr<target::AttachToTargetResult> result) {
    StopNavigationOnTarget(101, page_id_one_);
  }

  void OnAttachedToTargetTwo(
      std::unique_ptr<target::AttachToTargetResult> result) {
    StopNavigationOnTarget(102, page_id_two_);
  }

  void StopNavigationOnTarget(int message_id, std::string target_id) {
    // Avoid triggering Page.loadEventFired for about://blank if loading hasn't
    // finished yet.
    devtools_client_->GetTarget()->GetExperimental()->SendMessageToTarget(
        target::SendMessageToTargetParams::Builder()
            .SetTargetId(target_id)
            .SetMessage("{\"id\":" + std::to_string(message_id) +
                        ", \"method\": \"Page.stopLoading\"}")
            .Build());
  }

  void EnablePageOnTarget(int message_id, std::string target_id) {
    devtools_client_->GetTarget()->GetExperimental()->SendMessageToTarget(
        target::SendMessageToTargetParams::Builder()
            .SetTargetId(target_id)
            .SetMessage("{\"id\":" + std::to_string(message_id) +
                        ", \"method\": \"Page.enable\"}")
            .Build());
  }

  void NavigateTarget(int message_id, std::string target_id) {
    devtools_client_->GetTarget()->GetExperimental()->SendMessageToTarget(
        target::SendMessageToTargetParams::Builder()
            .SetTargetId(target_id)
            .SetMessage(
                "{\"id\":" + std::to_string(message_id) +
                ", \"method\": \"Page.navigate\", \"params\": {\"url\": \"" +
                embedded_test_server()->GetURL("/hello.html").spec() + "\"}}")
            .Build());
  }

  void MaybeSetCookieOnPageOne() {
    if (!page_one_loaded_ || !page_two_loaded_)
      return;

    devtools_client_->GetTarget()->GetExperimental()->SendMessageToTarget(
        target::SendMessageToTargetParams::Builder()
            .SetTargetId(page_id_one_)
            .SetMessage("{\"id\":401, \"method\": \"Runtime.evaluate\", "
                        "\"params\": {\"expression\": "
                        "\"document.cookie = 'foo=bar';\"}}")
            .Build());
  }

  void OnReceivedMessageFromTarget(
      const target::ReceivedMessageFromTargetParams& params) override {
    std::unique_ptr<base::Value> message =
        base::JSONReader::Read(params.GetMessage(), base::JSON_PARSE_RFC);
    const base::DictionaryValue* message_dict;
    if (!message || !message->GetAsDictionary(&message_dict)) {
      return;
    }

    std::string method;
    if (message_dict->GetString("method", &method) &&
        method == "Page.loadEventFired") {
      if (params.GetTargetId() == page_id_one_) {
        page_one_loaded_ = true;
      } else if (params.GetTargetId() == page_id_two_) {
        page_two_loaded_ = true;
      }
      MaybeSetCookieOnPageOne();
      return;
    }

    int message_id = 0;
    if (!message_dict->GetInteger("id", &message_id))
      return;
    const base::DictionaryValue* result_dict;
    if (message_dict->GetDictionary("result", &result_dict)) {
      if (message_id == 101) {
        // 101: Page.stopNavigation on target one.
        EXPECT_EQ(page_id_one_, params.GetTargetId());
        EnablePageOnTarget(201, page_id_one_);
      } else if (message_id == 102) {
        // 102: Page.stopNavigation on target two.
        EXPECT_EQ(page_id_two_, params.GetTargetId());
        EnablePageOnTarget(202, page_id_two_);
      } else if (message_id == 201) {
        // 201: Page.enable on target one.
        EXPECT_EQ(page_id_one_, params.GetTargetId());
        NavigateTarget(301, page_id_one_);
      } else if (message_id == 202) {
        // 202: Page.enable on target two.
        EXPECT_EQ(page_id_two_, params.GetTargetId());
        NavigateTarget(302, page_id_two_);
      } else if (message_id == 401) {
        // 401: Runtime.evaluate on target one.
        EXPECT_EQ(page_id_one_, params.GetTargetId());

        // TODO(alexclarke): Make some better bindings
        // for Target.SendMessageToTarget.
        devtools_client_->GetTarget()->GetExperimental()->SendMessageToTarget(
            target::SendMessageToTargetParams::Builder()
                .SetTargetId(page_id_two_)
                .SetMessage("{\"id\":402, \"method\": \"Runtime.evaluate\", "
                            "\"params\": {\"expression\": "
                            "\"document.cookie;\"}}")
                .Build());
      } else if (message_id == 402) {
        // 402: Runtime.evaluate on target two.
        EXPECT_EQ(page_id_two_, params.GetTargetId());

        // There's a nested result. We want the inner one.
        EXPECT_TRUE(result_dict->GetDictionary("result", &result_dict));

        std::string value;
        EXPECT_TRUE(result_dict->GetString("value", &value));
        EXPECT_EQ("", value) << "Page 2 should not share cookies from page one";

        devtools_client_->GetTarget()->GetExperimental()->CloseTarget(
            target::CloseTargetParams::Builder()
                .SetTargetId(page_id_one_)
                .Build(),
            base::Bind(&TargetDomainCreateTwoContexts::OnCloseTarget,
                       base::Unretained(this)));

        devtools_client_->GetTarget()->GetExperimental()->CloseTarget(
            target::CloseTargetParams::Builder()
                .SetTargetId(page_id_two_)
                .Build(),
            base::Bind(&TargetDomainCreateTwoContexts::OnCloseTarget,
                       base::Unretained(this)));

        devtools_client_->GetTarget()->GetExperimental()->RemoveObserver(this);
      }
    }
  }

  void OnCloseTarget(std::unique_ptr<target::CloseTargetResult> result) {
    page_close_count_++;

    if (page_close_count_ < 2)
      return;

    devtools_client_->GetTarget()->GetExperimental()->DisposeBrowserContext(
        target::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_one_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnCloseContext,
                   base::Unretained(this)));

    devtools_client_->GetTarget()->GetExperimental()->DisposeBrowserContext(
        target::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_two_)
            .Build(),
        base::Bind(&TargetDomainCreateTwoContexts::OnCloseContext,
                   base::Unretained(this)));
  }

  void OnCloseContext(
      std::unique_ptr<target::DisposeBrowserContextResult> result) {
    EXPECT_TRUE(result->GetSuccess());
    if (++context_closed_count_ < 2)
      return;

    FinishAsynchronousTest();
  }

 private:
  std::string context_id_one_;
  std::string context_id_two_;
  std::string page_id_one_;
  std::string page_id_two_;
  bool page_one_loaded_ = false;
  bool page_two_loaded_ = false;
  int page_close_count_ = 0;
  int context_closed_count_ = 0;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(TargetDomainCreateTwoContexts);

class HeadlessDevToolsNavigationControlTest
    : public HeadlessAsyncDevTooledBrowserTest,
      page::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    devtools_client_->GetPage()->GetExperimental()->SetControlNavigations(
        headless::page::SetControlNavigationsParams::Builder()
            .SetEnabled(true)
            .Build());
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnNavigationRequested(
      const headless::page::NavigationRequestedParams& params) override {
    navigation_requested_ = true;
    // Allow the navigation to proceed.
    devtools_client_->GetPage()->GetExperimental()->ProcessNavigation(
        headless::page::ProcessNavigationParams::Builder()
            .SetNavigationId(params.GetNavigationId())
            .SetResponse(headless::page::NavigationResponse::PROCEED)
            .Build());
  }

  void OnFrameStoppedLoading(
      const page::FrameStoppedLoadingParams& params) override {
    EXPECT_TRUE(navigation_requested_);
    FinishAsynchronousTest();
  }

 private:
  bool navigation_requested_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsNavigationControlTest);

class HeadlessCrashObserverTest : public HeadlessAsyncDevTooledBrowserTest,
                                  inspector::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    devtools_client_->GetInspector()->GetExperimental()->AddObserver(this);
    devtools_client_->GetInspector()->GetExperimental()->Enable(
        headless::inspector::EnableParams::Builder().Build());
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(content::kChromeUICrashURL);
  }

  void OnTargetCrashed(const inspector::TargetCrashedParams& params) override {
    FinishAsynchronousTest();
    render_process_exited_ = true;
  }

  // Make sure we don't fail because the renderer crashed!
  void RenderProcessExited(base::TerminationStatus status,
                           int exit_code) override {
#if defined(OS_WIN) || defined(OS_MACOSX)
    EXPECT_EQ(base::TERMINATION_STATUS_PROCESS_CRASHED, status);
#else
    EXPECT_EQ(base::TERMINATION_STATUS_ABNORMAL_TERMINATION, status);
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessCrashObserverTest);

class HeadlessDevToolsClientAttachTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void RunDevTooledTest() override {
    other_devtools_client_ = HeadlessDevToolsClient::Create();
    HeadlessDevToolsTarget* devtools_target =
        web_contents_->GetDevToolsTarget();

    // Try attaching: there's already a client attached.
    EXPECT_FALSE(devtools_target->AttachClient(other_devtools_client_.get()));
    EXPECT_TRUE(devtools_target->IsAttached());
    // Detach the existing client, attach the other client.
    devtools_target->DetachClient(devtools_client_.get());
    EXPECT_FALSE(devtools_target->IsAttached());
    EXPECT_TRUE(devtools_target->AttachClient(other_devtools_client_.get()));
    EXPECT_TRUE(devtools_target->IsAttached());

    // Now, let's make sure this devtools client works.
    other_devtools_client_->GetRuntime()->Evaluate(
        "24 * 7", base::Bind(&HeadlessDevToolsClientAttachTest::OnFirstResult,
                             base::Unretained(this)));
  }

  void OnFirstResult(std::unique_ptr<runtime::EvaluateResult> result) {
    int value;
    EXPECT_TRUE(result->GetResult()->HasValue());
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsInteger(&value));
    EXPECT_EQ(24 * 7, value);

    HeadlessDevToolsTarget* devtools_target =
        web_contents_->GetDevToolsTarget();

    // Try attach, then force-attach the original client.
    EXPECT_FALSE(devtools_target->AttachClient(devtools_client_.get()));
    devtools_target->ForceAttachClient(devtools_client_.get());
    EXPECT_TRUE(devtools_target->IsAttached());

    devtools_client_->GetRuntime()->Evaluate(
        "27 * 4", base::Bind(&HeadlessDevToolsClientAttachTest::OnSecondResult,
                             base::Unretained(this)));
  }

  void OnSecondResult(std::unique_ptr<runtime::EvaluateResult> result) {
    int value;
    EXPECT_TRUE(result->GetResult()->HasValue());
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsInteger(&value));
    EXPECT_EQ(27 * 4, value);

    // If everything worked, this call will not crash, since it
    // detaches devtools_client_.
    FinishAsynchronousTest();
  }

 protected:
  std::unique_ptr<HeadlessDevToolsClient> other_devtools_client_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientAttachTest);

class HeadlessDevToolsMethodCallErrorTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->GetExperimental()->RemoveObserver(this);
    devtools_client_->GetDOM()->GetDocument(
        base::Bind(&HeadlessDevToolsMethodCallErrorTest::OnGetDocument,
                   base::Unretained(this)));
  }

  void OnGetDocument(std::unique_ptr<dom::GetDocumentResult> result) {
    devtools_client_->GetDOM()->QuerySelector(
        dom::QuerySelectorParams::Builder()
            .SetNodeId(result->GetRoot()->GetNodeId())
            .SetSelector("<o_O>")
            .Build(),
        base::Bind(&HeadlessDevToolsMethodCallErrorTest::OnQuerySelector,
                   base::Unretained(this)));
  }

  void OnQuerySelector(std::unique_ptr<dom::QuerySelectorResult> result) {
    EXPECT_EQ(nullptr, result);
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsMethodCallErrorTest);

class HeadlessDevToolsNetworkBlockedUrlTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public page::Observer,
      public network::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    std::vector<std::string> blockedUrls;
    blockedUrls.push_back("dom_tree_test.css");
    devtools_client_->GetNetwork()->GetExperimental()->SetBlockedURLs(
        network::SetBlockedURLsParams::Builder().SetUrls(blockedUrls).Build());
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/dom_tree_test.html").spec());
  }

  std::string GetUrlPath(const std::string& url) const {
    GURL gurl(url);
    return gurl.path();
  }

  void OnRequestWillBeSent(
      const network::RequestWillBeSentParams& params) override {
    std::string path = GetUrlPath(params.GetRequest()->GetUrl());
    requests_to_be_sent_.push_back(path);
    request_id_to_path_[params.GetRequestId()] = path;
  }

  void OnResponseReceived(
      const network::ResponseReceivedParams& params) override {
    responses_received_.push_back(GetUrlPath(params.GetResponse()->GetUrl()));
  }

  void OnLoadingFailed(const network::LoadingFailedParams& failed) override {
    failures_.push_back(request_id_to_path_[failed.GetRequestId()]);
    EXPECT_EQ(network::BlockedReason::INSPECTOR, failed.GetBlockedReason());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    EXPECT_THAT(requests_to_be_sent_,
                ElementsAre("/dom_tree_test.html", "/dom_tree_test.css",
                            "/iframe.html"));
    EXPECT_THAT(responses_received_,
                ElementsAre("/dom_tree_test.html", "/iframe.html"));
    EXPECT_THAT(failures_, ElementsAre("/dom_tree_test.css"));
    FinishAsynchronousTest();
  }

  std::map<std::string, std::string> request_id_to_path_;
  std::vector<std::string> requests_to_be_sent_;
  std::vector<std::string> responses_received_;
  std::vector<std::string> failures_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsNetworkBlockedUrlTest);

namespace {
// Keep in sync with X_DevTools_Emulate_Network_Conditions_Client_Id defined in
// HTTPNames.json5.
const char kDevToolsEmulateNetworkConditionsClientId[] =
    "X-DevTools-Emulate-Network-Conditions-Client-Id";
}  // namespace

class DevToolsHeaderStrippingTest : public HeadlessAsyncDevTooledBrowserTest,
                                    public page::Observer,
                                    public network::Observer {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    base::RunLoop run_loop;
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    // Enable network domain in order to get DevTools to add the header.
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();
    devtools_client_->GetPage()->Navigate(
        "http://not-an-actual-domain.tld/hello.html");
  }

  ProtocolHandlerMap GetProtocolHandlers() override {
    const std::string kResponseBody = "<p>HTTP response body</p>";
    ProtocolHandlerMap protocol_handlers;
    protocol_handlers[url::kHttpScheme] =
        base::MakeUnique<TestProtocolHandler>(kResponseBody);
    test_handler_ = static_cast<TestProtocolHandler*>(
        protocol_handlers[url::kHttpScheme].get());
    return protocol_handlers;
  }

  void OnLoadEventFired(const page::LoadEventFiredParams&) override {
    EXPECT_FALSE(test_handler_->last_http_request_headers().IsEmpty());
    EXPECT_FALSE(test_handler_->last_http_request_headers().HasHeader(
        kDevToolsEmulateNetworkConditionsClientId));
    FinishAsynchronousTest();
  }

  TestProtocolHandler* test_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DevToolsHeaderStrippingTest);

class RawDevtoolsProtocolTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessDevToolsClient::RawProtocolListener {
 public:
  void RunDevTooledTest() override {
    devtools_client_->SetRawProtocolListener(this);

    base::DictionaryValue message;
    message.SetInteger("id", devtools_client_->GetNextRawDevToolsMessageId());
    message.SetString("method", "Runtime.evaluate");
    std::unique_ptr<base::DictionaryValue> params(new base::DictionaryValue());
    params->SetString("expression", "1+1");
    message.Set("params", std::move(params));
    devtools_client_->SendRawDevToolsMessage(message);
  }

  bool OnProtocolMessage(const std::string& devtools_agent_host_id,
                         const std::string& json_message,
                         const base::DictionaryValue& parsed_message) override {
    EXPECT_EQ(
        "{\"id\":1,\"result\":{\"result\":{\"type\":\"number\","
        "\"value\":2,\"description\":\"2\"}}}",
        json_message);

    FinishAsynchronousTest();
    return true;
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(RawDevtoolsProtocolTest);

class DevToolsAttachAndDetachNotifications
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  void DevToolsClientAttached() override { dev_tools_client_attached_ = true; }

  void RunDevTooledTest() override {
    EXPECT_TRUE(dev_tools_client_attached_);
    FinishAsynchronousTest();
  }

  void DevToolsClientDetached() override { dev_tools_client_detached_ = true; }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(dev_tools_client_detached_);
  }

 private:
  bool dev_tools_client_attached_ = false;
  bool dev_tools_client_detached_ = false;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DevToolsAttachAndDetachNotifications);

namespace {

std::string NormaliseJSON(const std::string& json) {
  std::unique_ptr<base::Value> parsed_json = base::JSONReader::Read(json);
  DCHECK(parsed_json);
  std::string normalized_json;
  base::JSONWriter::WriteWithOptions(
      *parsed_json, base::JSONWriter::OPTIONS_PRETTY_PRINT, &normalized_json);
  return normalized_json;
}

}  // namespace

class DomTreeExtractionBrowserTest : public HeadlessAsyncDevTooledBrowserTest,
                                     public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/dom_tree_test.html").spec());
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->Disable();
    devtools_client_->GetPage()->RemoveObserver(this);

    std::vector<std::string> css_whitelist = {
        "color",       "display",      "font-style", "font-family",
        "margin-left", "margin-right", "margin-top", "margin-bottom"};
    devtools_client_->GetDOMSnapshot()->GetExperimental()->GetSnapshot(
        dom_snapshot::GetSnapshotParams::Builder()
            .SetComputedStyleWhitelist(std::move(css_whitelist))
            .Build(),
        base::Bind(&DomTreeExtractionBrowserTest::OnGetSnapshotResult,
                   base::Unretained(this)));
  }

  void OnGetSnapshotResult(
      std::unique_ptr<dom_snapshot::GetSnapshotResult> result) {
    GURL::Replacements replace_port;
    replace_port.SetPortStr("");

    std::vector<std::unique_ptr<base::DictionaryValue>> dom_nodes(
        result->GetDomNodes()->size());

    // For convenience, flatten the dom tree into an array of dicts.
    for (size_t i = 0; i < result->GetDomNodes()->size(); i++) {
      dom_snapshot::DOMNode* node = (*result->GetDomNodes())[i].get();

      dom_nodes[i].reset(
          static_cast<base::DictionaryValue*>(node->Serialize().release()));
      base::DictionaryValue* node_dict = dom_nodes[i].get();

      // Frame IDs are random.
      if (node_dict->HasKey("frameId"))
        node_dict->SetString("frameId", "?");

      // Ports are random.
      std::string url;
      if (node_dict->GetString("baseURL", &url)) {
        node_dict->SetString("baseURL",
                             GURL(url).ReplaceComponents(replace_port).spec());
      }

      if (node_dict->GetString("documentURL", &url)) {
        node_dict->SetString("documentURL",
                             GURL(url).ReplaceComponents(replace_port).spec());
      }

      // Merge LayoutTreeNode data into the dictionary.
      int layout_node_index;
      if (node_dict->GetInteger("layoutNodeIndex", &layout_node_index)) {
        ASSERT_LE(0, layout_node_index);
        ASSERT_GT(result->GetLayoutTreeNodes()->size(),
                  static_cast<size_t>(layout_node_index));
        const std::unique_ptr<dom_snapshot::LayoutTreeNode>& layout_node =
            (*result->GetLayoutTreeNodes())[layout_node_index];

        node_dict->Set("boundingBox",
                       layout_node->GetBoundingBox()->Serialize());

        if (layout_node->HasLayoutText())
          node_dict->SetString("layoutText", layout_node->GetLayoutText());

        if (layout_node->HasStyleIndex())
          node_dict->SetInteger("styleIndex", layout_node->GetStyleIndex());

        if (layout_node->HasInlineTextNodes()) {
          std::unique_ptr<base::ListValue> inline_text_nodes(
              new base::ListValue());
          for (const std::unique_ptr<css::InlineTextBox>& inline_text_box :
               *layout_node->GetInlineTextNodes()) {
            size_t index = inline_text_nodes->GetSize();
            inline_text_nodes->Set(index, inline_text_box->Serialize());
          }
          node_dict->Set("inlineTextNodes", std::move(inline_text_nodes));
        }
      }
    }

    std::vector<std::unique_ptr<base::DictionaryValue>> computed_styles(
        result->GetComputedStyles()->size());

    for (size_t i = 0; i < result->GetComputedStyles()->size(); i++) {
      std::unique_ptr<base::DictionaryValue> style(new base::DictionaryValue());
      for (const auto& style_property :
           *(*result->GetComputedStyles())[i]->GetProperties()) {
        style->SetString(style_property->GetName(), style_property->GetValue());
      }
      computed_styles[i] = std::move(style);
    }

    // TODO(eseckler): Extract expectation strings into external files to make
    // them easier to maintain.
    const std::vector<std::string> expected_dom_nodes = {
        R"raw_string({
           "backendNodeId": 3,
           "baseURL": "http://127.0.0.1/dom_tree_test.html",
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childNodeIndexes": [ 1 ],
           "documentURL": "http://127.0.0.1/dom_tree_test.html",
           "layoutNodeIndex": 0,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 4,
           "boundingBox": {
              "height": 600.0,
              "width": 800.0,
              "x": 0.0,
              "y": 0.0
           },
           "childNodeIndexes": [ 2, 9, 10 ],
           "frameId": "?",
           "layoutNodeIndex": 1,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 5,
           "childNodeIndexes": [ 3, 4, 6, 7, 8 ],
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 6,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 7,
           "childNodeIndexes": [ 5 ],
           "nodeName": "TITLE",
           "nodeType": 1,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 8,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello world!"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 9,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "href",
              "value": "dom_tree_test.css"
           }, {
              "name": "rel",
              "value": "stylesheet"
           }, {
              "name": "type",
              "value": "text/css"
           } ],
           "backendNodeId": 10,
           "nodeName": "LINK",
           "nodeType": 1,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 11,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 12,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 13,
           "boundingBox": {
              "height": 584.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childNodeIndexes": [ 11, 12 ],
           "layoutNodeIndex": 2,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 1
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 14,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "id",
              "value": "id1"
           } ],
           "backendNodeId": 15,
           "boundingBox": {
              "height": 354.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childNodeIndexes": [ 13, 14, 16, 17, 26, 27, 49 ],
           "layoutNodeIndex": 3,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 16,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "class",
              "value": "red"
           } ],
           "backendNodeId": 17,
           "boundingBox": {
              "height": 32.0,
              "width": 784.0,
              "x": 8.0,
              "y": 8.0
           },
           "childNodeIndexes": [ 15 ],
           "layoutNodeIndex": 4,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 2
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 18,
           "boundingBox": {
              "height": 32.0,
              "width": 320.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 32.0,
                 "width": 320.0,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 10,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 5,
           "layoutText": "Some text.",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some text.",
           "styleIndex": 2
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 19,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "src",
              "value": "/iframe.html"
           }, {
              "name": "width",
              "value": "400"
           }, {
              "name": "height",
              "value": "200"
           } ],
           "backendNodeId": 20,
           "boundingBox": {
              "height": 205.0,
              "width": 404.0,
              "x": 8.0,
              "y": 61.0
           },
           "contentDocumentIndex": 18,
           "frameId": "?",
           "layoutNodeIndex": 6,
           "nodeName": "IFRAME",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 3
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 21,
           "baseURL": "http://127.0.0.1/iframe.html",
           "boundingBox": {
              "height": 200.0,
              "width": 400.0,
              "x": 0.0,
              "y": 0.0
           },
           "childNodeIndexes": [ 19 ],
           "documentURL": "http://127.0.0.1/iframe.html",
           "layoutNodeIndex": 7,
           "nodeName": "#document",
           "nodeType": 9,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 22,
           "boundingBox": {
              "height": 200.0,
              "width": 400.0,
              "x": 10.0,
              "y": 63.0
           },
           "childNodeIndexes": [ 20, 21 ],
           "frameId": "?",
           "layoutNodeIndex": 8,
           "nodeName": "HTML",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 4
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 23,
           "nodeName": "HEAD",
           "nodeType": 1,
           "nodeValue": ""
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 24,
           "boundingBox": {
              "height": 171.0,
              "width": 384.0,
              "x": 18.0,
              "y": 71.0
           },
           "childNodeIndexes": [ 22, 23, 25 ],
           "layoutNodeIndex": 9,
           "nodeName": "BODY",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 5
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 25,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 26,
           "boundingBox": {
              "height": 37.0,
              "width": 384.0,
              "x": 18.0,
              "y": 71.0
           },
           "childNodeIndexes": [ 24 ],
           "layoutNodeIndex": 10,
           "nodeName": "H1",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 6
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 27,
           "boundingBox": {
              "height": 36.0,
              "width": 308.0,
              "x": 8.0,
              "y": 8.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 36.0,
                 "width": 307.734375,
                 "x": 8.0,
                 "y": 8.0
              },
              "numCharacters": 22,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 11,
           "layoutText": "Hello from the iframe!",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Hello from the iframe!",
           "styleIndex": 6
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 28,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n\n\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 29,
           "boundingBox": {
              "height": 0.0,
              "width": 0.0,
              "x": 0.0,
              "y": 0.0
           },
           "layoutNodeIndex": 12,
           "layoutText": "\n",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "id",
              "value": "id2"
           } ],
           "backendNodeId": 30,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childNodeIndexes": [ 28, 29, 48 ],
           "layoutNodeIndex": 13,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 31,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n  "
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "id",
              "value": "id3"
           } ],
           "backendNodeId": 32,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childNodeIndexes": [ 30, 31, 47 ],
           "layoutNodeIndex": 14,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 33,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n    "
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "id",
              "value": "id4"
           } ],
           "backendNodeId": 34,
           "boundingBox": {
              "height": 97.0,
              "width": 784.0,
              "x": 8.0,
              "y": 265.0
           },
           "childNodeIndexes": [ 32, 33, 35, 36, 38, 39, 40, 41, 46 ],
           "layoutNodeIndex": 15,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 35,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n      "
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "href",
              "value": "https://www.google.com"
           } ],
           "backendNodeId": 36,
           "boundingBox": {
              "height": 17.0,
              "width": 112.0,
              "x": 8.0,
              "y": 265.0
           },
           "childNodeIndexes": [ 34 ],
           "layoutNodeIndex": 16,
           "nodeName": "A",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 7
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 37,
           "boundingBox": {
              "height": 17.0,
              "width": 112.0,
              "x": 8.0,
              "y": 265.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 112.0,
                 "x": 8.0,
                 "y": 265.4375
              },
              "numCharacters": 7,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 17,
           "layoutText": "Google!",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Google!",
           "styleIndex": 7
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 38,
           "boundingBox": {
              "height": 0.0,
              "width": 0.0,
              "x": 0.0,
              "y": 0.0
           },
           "layoutNodeIndex": 18,
           "layoutText": "\n      ",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n      ",
           "styleIndex": 0
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 39,
           "boundingBox": {
              "height": 17.0,
              "width": 784.0,
              "x": 8.0,
              "y": 297.0
           },
           "childNodeIndexes": [ 37 ],
           "layoutNodeIndex": 19,
           "nodeName": "P",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 8
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 40,
           "boundingBox": {
              "height": 17.0,
              "width": 192.0,
              "x": 8.0,
              "y": 297.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 192.0,
                 "x": 8.0,
                 "y": 297.4375
              },
              "numCharacters": 12,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 20,
           "layoutText": "A paragraph!",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "A paragraph!",
           "styleIndex": 8
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 41,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n      "
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 42,
           "boundingBox": {
              "height": 0.0,
              "width": 0.0,
              "x": 0.0,
              "y": 0.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 0.0,
                 "x": 8.0,
                 "y": 329.4375
              },
              "numCharacters": 1,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 21,
           "layoutText": "\n",
           "nodeName": "BR",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 3
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 43,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n      "
        }
        )raw_string",

        R"raw_string({
           "attributes": [ {
              "name": "class",
              "value": "green"
           } ],
           "backendNodeId": 44,
           "boundingBox": {
              "height": 17.0,
              "width": 784.0,
              "x": 8.0,
              "y": 345.0
           },
           "childNodeIndexes": [ 42, 43, 45 ],
           "layoutNodeIndex": 22,
           "nodeName": "DIV",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 9
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 45,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 8.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 80.0,
                 "x": 8.0,
                 "y": 345.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 23,
           "layoutText": "Some ",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "Some ",
           "styleIndex": 9
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 46,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 88.0,
              "y": 345.0
           },
           "childNodeIndexes": [ 44 ],
           "layoutNodeIndex": 24,
           "nodeName": "EM",
           "nodeType": 1,
           "nodeValue": "",
           "styleIndex": 10
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 47,
           "boundingBox": {
              "height": 17.0,
              "width": 80.0,
              "x": 88.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 80.0,
                 "x": 88.0,
                 "y": 345.4375
              },
              "numCharacters": 5,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 25,
           "layoutText": "green",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "green",
           "styleIndex": 10
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 48,
           "boundingBox": {
              "height": 17.0,
              "width": 128.0,
              "x": 168.0,
              "y": 345.0
           },
           "inlineTextNodes": [ {
              "boundingBox": {
                 "height": 16.0,
                 "width": 128.0,
                 "x": 168.0,
                 "y": 345.4375
              },
              "numCharacters": 8,
              "startCharacterIndex": 0
           } ],
           "layoutNodeIndex": 26,
           "layoutText": " text...",
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": " text...",
           "styleIndex": 9
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 49,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n    "
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 50,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n  "
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 51,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n"
        }
        )raw_string",

        R"raw_string({
           "backendNodeId": 52,
           "nodeName": "#text",
           "nodeType": 3,
           "nodeValue": "\n\n\n"
        }
        )raw_string"};

    EXPECT_EQ(expected_dom_nodes.size(), dom_nodes.size());

    for (size_t i = 0; i < dom_nodes.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(
          *dom_nodes[i], base::JSONWriter::OPTIONS_PRETTY_PRINT, &result_json);

      ASSERT_LT(i, expected_dom_nodes.size());
      EXPECT_EQ(NormaliseJSON(expected_dom_nodes[i]), result_json)
          << " Node # " << i;
    }

    const std::vector<std::string> expected_styles = {
        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "8px",
           "margin-left": "8px",
           "margin-right": "8px",
           "margin-top": "8px"
        })raw_string",

        R"raw_string({
           "color": "rgb(255, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "8px",
           "margin-left": "8px",
           "margin-right": "8px",
           "margin-top": "8px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "\"Times New Roman\"",
           "font-style": "normal",
           "margin-bottom": "21.44px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "21.44px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 238)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 0, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "16px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "16px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "block",
           "font-family": "ahem",
           "font-style": "normal",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        })raw_string",

        R"raw_string({
           "color": "rgb(0, 128, 0)",
           "display": "inline",
           "font-family": "ahem",
           "font-style": "italic",
           "margin-bottom": "0px",
           "margin-left": "0px",
           "margin-right": "0px",
           "margin-top": "0px"
        }
        )raw_string"};

    EXPECT_EQ(expected_styles.size(), computed_styles.size());

    for (size_t i = 0; i < computed_styles.size(); i++) {
      std::string result_json;
      base::JSONWriter::WriteWithOptions(*computed_styles[i],
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &result_json);

      ASSERT_LT(i, expected_styles.size());
      EXPECT_EQ(NormaliseJSON(expected_styles[i]), result_json)
          << " Style # " << i;
    }

    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(DomTreeExtractionBrowserTest);

}  // namespace headless
