// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "content/public/test/browser_test.h"
#include "headless/public/domains/network.h"
#include "headless/public/domains/page.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace headless {

#define DEVTOOLS_CLIENT_TEST_F(TEST_FIXTURE_NAME)                        \
  IN_PROC_BROWSER_TEST_F(TEST_FIXTURE_NAME, RunAsyncTest) { RunTest(); } \
  class AsyncHeadlessBrowserTestNeedsSemicolon##TEST_FIXTURE_NAME {}

// A test fixture which attaches a devtools client before starting the test.
class HeadlessDevToolsClientTest : public HeadlessBrowserTest,
                                   public HeadlessWebContents::Observer {
 public:
  HeadlessDevToolsClientTest()
      : devtools_client_(HeadlessDevToolsClient::Create()) {}
  ~HeadlessDevToolsClientTest() override {}

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override {
    EXPECT_TRUE(web_contents_->GetDevToolsTarget());
    web_contents_->GetDevToolsTarget()->AttachClient(devtools_client_.get());
    RunDevToolsClientTest();
  }

  virtual void RunDevToolsClientTest() = 0;

 protected:
  void RunTest() {
    web_contents_ =
        browser()->CreateWebContents(GURL("about:blank"), gfx::Size(800, 600));
    web_contents_->AddObserver(this);

    RunAsynchronousTest();

    web_contents_->GetDevToolsTarget()->DetachClient(devtools_client_.get());
    web_contents_->RemoveObserver(this);
    web_contents_->Close();
    web_contents_ = nullptr;
  }

  HeadlessWebContents* web_contents_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client_;
};

class HeadlessDevToolsClientNavigationTest : public HeadlessDevToolsClientTest,
                                             page::ExperimentalObserver {
 public:
  void RunDevToolsClientTest() override {
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

DEVTOOLS_CLIENT_TEST_F(HeadlessDevToolsClientNavigationTest);

class HeadlessDevToolsClientEvalTest : public HeadlessDevToolsClientTest {
 public:
  void RunDevToolsClientTest() override {
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

DEVTOOLS_CLIENT_TEST_F(HeadlessDevToolsClientEvalTest);

class HeadlessDevToolsClientCallbackTest : public HeadlessDevToolsClientTest {
 public:
  HeadlessDevToolsClientCallbackTest() : first_result_received_(false) {}

  void RunDevToolsClientTest() override {
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

DEVTOOLS_CLIENT_TEST_F(HeadlessDevToolsClientCallbackTest);

class HeadlessDevToolsClientObserverTest : public HeadlessDevToolsClientTest,
                                           network::Observer {
 public:
  void RunDevToolsClientTest() override {
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

DEVTOOLS_CLIENT_TEST_F(HeadlessDevToolsClientObserverTest);

class HeadlessDevToolsClientExperimentalTest
    : public HeadlessDevToolsClientTest,
      page::ExperimentalObserver {
 public:
  void RunDevToolsClientTest() override {
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

DEVTOOLS_CLIENT_TEST_F(HeadlessDevToolsClientExperimentalTest);

}  // namespace headless
