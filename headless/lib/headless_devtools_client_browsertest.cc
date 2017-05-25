// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/browser.h"
#include "headless/public/devtools/domains/dom.h"
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
// https://crbug.com/726288: Mac does not currently support repositioning.
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

}  // namespace headless
