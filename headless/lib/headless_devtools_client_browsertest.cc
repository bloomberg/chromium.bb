// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/json/json_reader.h"
#include "content/public/test/browser_test.h"
#include "headless/public/domains/browser.h"
#include "headless/public/domains/network.h"
#include "headless/public/domains/page.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace headless {

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
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(std::move(params));
  }

  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    devtools_client_->GetPage()->GetExperimental()->RemoveObserver(this);
    FinishAsynchronousTest();
  }

  // Check that events with no parameters still get a parameters object.
  void OnFrameResized(const page::FrameResizedParams& params) override {}
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientNavigationTest);

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
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable();
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
    // Check that experimental commands require parameter objects.
    devtools_client_->GetRuntime()->GetExperimental()->Run(
        runtime::RunParams::Builder().Build());

    devtools_client_->GetPage()->GetExperimental()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnFrameStoppedLoading(
      const page::FrameStoppedLoadingParams& params) override {
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessDevToolsClientExperimentalTest);

class BrowserDomainCreateAndDeletePageTest
    : public HeadlessAsyncDevTooledBrowserTest {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    EXPECT_EQ(1u, browser()->GetAllWebContents().size());

    devtools_client_->GetBrowser()->GetExperimental()->CreateTarget(
        browser::CreateTargetParams::Builder()
            .SetInitialUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetWidth(1)
            .SetHeight(1)
            .Build(),
        base::Bind(&BrowserDomainCreateAndDeletePageTest::OnCreateTargetResult,
                   base::Unretained(this)));
  }

  void OnCreateTargetResult(
      std::unique_ptr<browser::CreateTargetResult> result) {
    EXPECT_EQ(2u, browser()->GetAllWebContents().size());

    devtools_client_->GetBrowser()->GetExperimental()->CloseTarget(
        browser::CloseTargetParams::Builder()
            .SetTargetId(result->GetTargetId())
            .Build(),
        base::Bind(&BrowserDomainCreateAndDeletePageTest::OnCloseTargetResult,
                   base::Unretained(this)));
  }

  void OnCloseTargetResult(std::unique_ptr<browser::CloseTargetResult> result) {
    EXPECT_EQ(1u, browser()->GetAllWebContents().size());
    FinishAsynchronousTest();
  }
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(BrowserDomainCreateAndDeletePageTest);

class BrowserDomainDisposeContextFailsIfInUse
    : public HeadlessAsyncDevTooledBrowserTest {
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    EXPECT_EQ(1u, browser()->GetAllWebContents().size());
    devtools_client_->GetBrowser()->GetExperimental()->CreateBrowserContext(
        browser::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&BrowserDomainDisposeContextFailsIfInUse::OnContextCreated,
                   base::Unretained(this)));
  }

  void OnContextCreated(
      std::unique_ptr<browser::CreateBrowserContextResult> result) {
    context_id_ = result->GetBrowserContextId();

    devtools_client_->GetBrowser()->GetExperimental()->CreateTarget(
        browser::CreateTargetParams::Builder()
            .SetInitialUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(
            &BrowserDomainDisposeContextFailsIfInUse::OnCreateTargetResult,
            base::Unretained(this)));
  }

  void OnCreateTargetResult(
      std::unique_ptr<browser::CreateTargetResult> result) {
    page_id_ = result->GetTargetId();

    devtools_client_->GetBrowser()->GetExperimental()->DisposeBrowserContext(
        browser::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(&BrowserDomainDisposeContextFailsIfInUse::
                       OnDisposeBrowserContextResult,
                   base::Unretained(this)));
  }

  void OnDisposeBrowserContextResult(
      std::unique_ptr<browser::DisposeBrowserContextResult> result) {
    EXPECT_FALSE(result->GetSuccess());

    // Close the page and try again.
    devtools_client_->GetBrowser()->GetExperimental()->CloseTarget(
        browser::CloseTargetParams::Builder().SetTargetId(page_id_).Build(),
        base::Bind(
            &BrowserDomainDisposeContextFailsIfInUse::OnCloseTargetResult,
            base::Unretained(this)));
  }

  void OnCloseTargetResult(std::unique_ptr<browser::CloseTargetResult> result) {
    EXPECT_TRUE(result->GetSuccess());

    devtools_client_->GetBrowser()->GetExperimental()->DisposeBrowserContext(
        browser::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_)
            .Build(),
        base::Bind(&BrowserDomainDisposeContextFailsIfInUse::
                       OnDisposeBrowserContextResult2,
                   base::Unretained(this)));
  }

  void OnDisposeBrowserContextResult2(
      std::unique_ptr<browser::DisposeBrowserContextResult> result) {
    EXPECT_TRUE(result->GetSuccess());
    FinishAsynchronousTest();
  }

 private:
  std::string context_id_;
  std::string page_id_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(BrowserDomainDisposeContextFailsIfInUse);

class BrowserDomainCreateTwoContexts : public HeadlessAsyncDevTooledBrowserTest,
                                       public browser::ExperimentalObserver {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());

    devtools_client_->GetBrowser()->GetExperimental()->AddObserver(this);
    devtools_client_->GetBrowser()->GetExperimental()->CreateBrowserContext(
        browser::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnContextOneCreated,
                   base::Unretained(this)));

    devtools_client_->GetBrowser()->GetExperimental()->CreateBrowserContext(
        browser::CreateBrowserContextParams::Builder().Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnContextTwoCreated,
                   base::Unretained(this)));
  }

  void OnContextOneCreated(
      std::unique_ptr<browser::CreateBrowserContextResult> result) {
    context_id_one_ = result->GetBrowserContextId();
    MaybeCreatePages();
  }

  void OnContextTwoCreated(
      std::unique_ptr<browser::CreateBrowserContextResult> result) {
    context_id_two_ = result->GetBrowserContextId();
    MaybeCreatePages();
  }

  void MaybeCreatePages() {
    if (context_id_one_.empty() || context_id_two_.empty())
      return;

    devtools_client_->GetBrowser()->GetExperimental()->CreateTarget(
        browser::CreateTargetParams::Builder()
            .SetInitialUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetBrowserContextId(context_id_one_)
            .Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnCreateTargetOneResult,
                   base::Unretained(this)));

    devtools_client_->GetBrowser()->GetExperimental()->CreateTarget(
        browser::CreateTargetParams::Builder()
            .SetInitialUrl(embedded_test_server()->GetURL("/hello.html").spec())
            .SetBrowserContextId(context_id_two_)
            .Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnCreateTargetTwoResult,
                   base::Unretained(this)));
  }

  void OnCreateTargetOneResult(
      std::unique_ptr<browser::CreateTargetResult> result) {
    page_id_one_ = result->GetTargetId();
    MaybeTestIsolation();
  }

  void OnCreateTargetTwoResult(
      std::unique_ptr<browser::CreateTargetResult> result) {
    page_id_two_ = result->GetTargetId();
    MaybeTestIsolation();
  }

  void MaybeTestIsolation() {
    if (page_id_one_.empty() || page_id_two_.empty())
      return;

    devtools_client_->GetBrowser()->GetExperimental()->Attach(
        browser::AttachParams::Builder().SetTargetId(page_id_one_).Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnAttachedOne,
                   base::Unretained(this)));

    devtools_client_->GetBrowser()->GetExperimental()->Attach(
        browser::AttachParams::Builder().SetTargetId(page_id_two_).Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnAttachedTwo,
                   base::Unretained(this)));
  }

  void OnAttachedOne(std::unique_ptr<browser::AttachResult> result) {
    devtools_client_->GetBrowser()->GetExperimental()->SendMessage(
        browser::SendMessageParams::Builder()
            .SetTargetId(page_id_one_)
            .SetMessage("{\"id\":101, \"method\": \"Page.enable\"}")
            .Build());
  }

  void OnAttachedTwo(std::unique_ptr<browser::AttachResult> result) {
    devtools_client_->GetBrowser()->GetExperimental()->SendMessage(
        browser::SendMessageParams::Builder()
            .SetTargetId(page_id_two_)
            .SetMessage("{\"id\":102, \"method\": \"Page.enable\"}")
            .Build());
  }

  void MaybeSetCookieOnPageOne() {
    if (!page_one_loaded_ || !page_two_loaded_)
      return;

    devtools_client_->GetBrowser()->GetExperimental()->SendMessage(
        browser::SendMessageParams::Builder()
            .SetTargetId(page_id_one_)
            .SetMessage("{\"id\":201, \"method\": \"Runtime.evaluate\", "
                        "\"params\": {\"expression\": "
                        "\"document.cookie = 'foo=bar';\"}}")
            .Build());
  }

  void OnDispatchMessage(
      const browser::DispatchMessageParams& params) override {
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
    const base::DictionaryValue* result_dict;
    if (message_dict->GetDictionary("result", &result_dict)) {
      // There's a nested result. We want the inner one.
      if (!result_dict->GetDictionary("result", &result_dict))
        return;
      std::string value;
      if (params.GetTargetId() == page_id_one_) {
        // TODO(alexclarke): Make some better bindings for Browser.sendMessage.
        devtools_client_->GetBrowser()->GetExperimental()->SendMessage(
            browser::SendMessageParams::Builder()
                .SetTargetId(page_id_two_)
                .SetMessage("{\"id\":202, \"method\": \"Runtime.evaluate\", "
                            "\"params\": {\"expression\": "
                            "\"document.cookie;\"}}")
                .Build());
      } else if (params.GetTargetId() == page_id_two_ &&
                 result_dict->GetString("value", &value)) {
        EXPECT_EQ("", value) << "Page 2 should not share cookies from page one";

        devtools_client_->GetBrowser()->GetExperimental()->CloseTarget(
            browser::CloseTargetParams::Builder()
                .SetTargetId(page_id_one_)
                .Build(),
            base::Bind(&BrowserDomainCreateTwoContexts::OnCloseTarget,
                       base::Unretained(this)));

        devtools_client_->GetBrowser()->GetExperimental()->CloseTarget(
            browser::CloseTargetParams::Builder()
                .SetTargetId(page_id_two_)
                .Build(),
            base::Bind(&BrowserDomainCreateTwoContexts::OnCloseTarget,
                       base::Unretained(this)));

        devtools_client_->GetBrowser()->GetExperimental()->RemoveObserver(this);
      }
    }
  }

  void OnCloseTarget(std::unique_ptr<browser::CloseTargetResult> result) {
    page_close_count_++;

    if (page_close_count_ < 2)
      return;

    devtools_client_->GetBrowser()->GetExperimental()->DisposeBrowserContext(
        browser::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_one_)
            .Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnCloseContext,
                   base::Unretained(this)));

    devtools_client_->GetBrowser()->GetExperimental()->DisposeBrowserContext(
        browser::DisposeBrowserContextParams::Builder()
            .SetBrowserContextId(context_id_two_)
            .Build(),
        base::Bind(&BrowserDomainCreateTwoContexts::OnCloseContext,
                   base::Unretained(this)));
  }

  void OnCloseContext(
      std::unique_ptr<browser::DisposeBrowserContextResult> result) {
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

HEADLESS_ASYNC_DEVTOOLED_TEST_F(BrowserDomainCreateTwoContexts);

}  // namespace headless
