// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace web {

namespace {

// Verifies correctness of |NavigationContext| for new page navigation passed to
// |DidFinishNavigation|.
ACTION_P2(VerifyNewPageContext, web_state, url) {
  NavigationContext* context = arg0;
  ASSERT_TRUE(context);
  EXPECT_EQ(web_state, context->GetWebState());
  EXPECT_EQ(url, context->GetUrl());
  EXPECT_FALSE(context->IsSamePage());
  EXPECT_FALSE(context->IsErrorPage());
}

// Verifies correctness of |NavigationContext| for same page navigation passed
// to |DidFinishNavigation|.
ACTION_P2(VerifySamePageContext, web_state, url) {
  NavigationContext* context = arg0;
  ASSERT_TRUE(context);
  EXPECT_EQ(web_state, context->GetWebState());
  EXPECT_EQ(url, context->GetUrl());
  EXPECT_TRUE(context->IsSamePage());
  EXPECT_FALSE(context->IsErrorPage());
}

// Mocks DidFinishNavigation navigation callback.
class WebStateObserverMock : public WebStateObserver {
 public:
  WebStateObserverMock(WebState* web_state) : WebStateObserver(web_state) {}
  MOCK_METHOD1(DidFinishNavigation, void(NavigationContext* context));
};

}  // namespace

using test::HttpServer;
using testing::_;

// Test fixture for WebStateDelegate::DidFinishNavigation integration tests.
class DidFinishNavigationTest : public WebIntTest {
  void SetUp() override {
    WebIntTest::SetUp();
    observer_ = base::MakeUnique<WebStateObserverMock>(web_state());
  }

 protected:
  std::unique_ptr<WebStateObserverMock> observer_;
};

// Tests successful navigation to a new page.
TEST_F(DidFinishNavigationTest, NewPageNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);
}

// Tests user-initiated hash change.
TEST_F(DidFinishNavigationTest, UserInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform same-page navigation.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySamePageContext(web_state(), hash_url));
  LoadUrl(hash_url);

  // Perform same-page navigation by going back.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySamePageContext(web_state(), url));
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });
}

// Tests renderer-initiated hash change.
TEST_F(DidFinishNavigationTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySamePageContext(web_state(), hash_url));
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_F(DidFinishNavigationTest, StateNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform push state using JavaScript.
  const GURL push_url = HttpServer::MakeUrl("http://chromium.test/test.html");
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySamePageContext(web_state(), push_url));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = HttpServer::MakeUrl("http://chromium.test/1.html");
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySamePageContext(web_state(), replace_url));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_F(DidFinishNavigationTest, NativeContentNavigation) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);
}

}  // namespace web
