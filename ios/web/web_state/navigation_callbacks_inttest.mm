// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
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

const char kExpectedMimeType[] = "text/html";

// Verifies correctness of |NavigationContext| for new page navigation passed to
// |DidFinishNavigation|.
ACTION_P2(VerifyNewPageContext, web_state, url) {
  NavigationContext* context = arg0;
  ASSERT_TRUE(context);
  EXPECT_EQ(web_state, context->GetWebState());
  EXPECT_EQ(url, context->GetUrl());
  EXPECT_FALSE(context->IsSameDocument());
  EXPECT_FALSE(context->IsErrorPage());
  ASSERT_TRUE(context->GetResponseHeaders());
  std::string mime_type;
  context->GetResponseHeaders()->GetMimeType(&mime_type);
  EXPECT_EQ(kExpectedMimeType, mime_type);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| for same page navigation passed
// to |DidFinishNavigation|.
ACTION_P2(VerifySameDocumentContext, web_state, url) {
  NavigationContext* context = arg0;
  ASSERT_TRUE(context);
  EXPECT_EQ(web_state, context->GetWebState());
  EXPECT_EQ(url, context->GetUrl());
  EXPECT_TRUE(context->IsSameDocument());
  EXPECT_FALSE(context->IsErrorPage());
  EXPECT_FALSE(context->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| for new page navigation to native
// URLs passed to |DidFinishNavigation|.
ACTION_P2(VerifyNewNativePageContext, web_state, url) {
  NavigationContext* context = arg0;
  ASSERT_TRUE(context);
  EXPECT_EQ(web_state, context->GetWebState());
  EXPECT_EQ(url, context->GetUrl());
  EXPECT_FALSE(context->IsSameDocument());
  EXPECT_FALSE(context->IsErrorPage());
  EXPECT_FALSE(context->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Mocks DidFinishNavigation navigation callback.
class WebStateObserverMock : public WebStateObserver {
 public:
  WebStateObserverMock(WebState* web_state) : WebStateObserver(web_state) {}
  MOCK_METHOD1(DidFinishNavigation, void(NavigationContext* context));
  MOCK_METHOD1(ProvisionalNavigationStarted, void(const GURL& url));
};

}  // namespace

using test::HttpServer;
using testing::_;

// Test fixture for WebStateDelegate::ProvisionalNavigationStarted and
// WebStateDelegate::DidFinishNavigation integration tests.
class StartAndFinishNavigationTest : public WebIntTest {
  void SetUp() override {
    WebIntTest::SetUp();
    observer_ = base::MakeUnique<WebStateObserverMock>(web_state());
  }

 protected:
  std::unique_ptr<WebStateObserverMock> observer_;
};

// Tests successful navigation to a new page.
TEST_F(StartAndFinishNavigationTest, NewPageNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);
}

// Tests user-initiated hash change.
TEST_F(StartAndFinishNavigationTest, UserInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform same-page navigation.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentContext(web_state(), hash_url));
  LoadUrl(hash_url);

  // Perform same-page navigation by going back.
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentContext(web_state(), url));
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });
}

// Tests renderer-initiated hash change.
TEST_F(StartAndFinishNavigationTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentContext(web_state(), hash_url));
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_F(StartAndFinishNavigationTest, StateNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageContext(web_state(), url));
  LoadUrl(url);

  // Perform push state using JavaScript.
  const GURL push_url = HttpServer::MakeUrl("http://chromium.test/test.html");
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentContext(web_state(), push_url));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = HttpServer::MakeUrl("http://chromium.test/1.html");
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentContext(web_state(), replace_url));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_F(StartAndFinishNavigationTest, NativeContentNavigation) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(*observer_, ProvisionalNavigationStarted(_));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewNativePageContext(web_state(), url));
  LoadUrl(url);
}

}  // namespace web
