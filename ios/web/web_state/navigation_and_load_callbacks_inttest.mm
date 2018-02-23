// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/scoped_observer.h"
#include "base/strings/stringprintf.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/test/fakes/test_native_content.h"
#import "ios/web/public/test/fakes/test_native_content_provider.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_policy_decider.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {

const char kTestFormValue[] = "inttestvalue";
const char kTestPageText[] = "landing!";
const char kExpectedMimeType[] = "text/html";

const char kDownloadMimeType[] = "application/vnd.test";

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P5(VerifyPageStartedContext,
          web_state,
          url,
          transition,
          context,
          nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  ui::PageTransition actual_transition = (*context)->GetPageTransition();
  EXPECT_TRUE(PageTransitionCoreTypeIs(transition, actual_transition))
      << "Got unexpected transition: " << actual_transition;
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetPendingItem();
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifyNewPageFinishedContext,
          web_state,
          url,
          mime_type,
          context,
          nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_TRUE((*context)->GetResponseHeaders());
  std::string actual_mime_type;
  (*context)->GetResponseHeaders()->GetMimeType(&actual_mime_type);
  ASSERT_TRUE(web_state->IsLoading());
  EXPECT_EQ(mime_type, actual_mime_type);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for failed navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P4(VerifyErrorFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  // The error code will be different on bots and for local runs. Allow both.
  NSInteger error_code = (*context)->GetError().code;
  EXPECT_TRUE(error_code == NSURLErrorNetworkConnectionLost);
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  ASSERT_FALSE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_FALSE(item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for navigations via POST
// HTTP methods passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P5(VerifyPostStartedContext,
          web_state,
          url,
          context,
          nav_id,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_TRUE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_EQ(renderer_initiated, (*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  // TODO(crbug.com/676129): Reload does not create a pending item. Remove this
  // workaround once the bug is fixed. The slim navigation manager fixes this
  // bug.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled() ||
      !ui::PageTransitionTypeIncludingQualifiersIs(
          ui::PageTransition::PAGE_TRANSITION_RELOAD,
          (*context)->GetPageTransition())) {
    NavigationManager* navigation_manager = web_state->GetNavigationManager();
    NavigationItem* item = navigation_manager->GetPendingItem();
    EXPECT_EQ(url, item->GetURL());
  }
}

// Verifies correctness of |NavigationContext| (|arg1|) for navigations via POST
// HTTP methods passed to |DidFinishNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P5(VerifyPostFinishedContext,
          web_state,
          url,
          context,
          nav_id,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_TRUE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_EQ(renderer_initiated, (*context)->IsRendererInitiated());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for same page navigation
// passed to |DidFinishNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P6(VerifySameDocumentStartedContext,
          web_state,
          url,
          context,
          nav_id,
          page_transition,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
}

// Verifies correctness of |NavigationContext| (|arg1|) for same page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P6(VerifySameDocumentFinishedContext,
          web_state,
          url,
          context,
          nav_id,
          page_transition,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// to native URLs passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P4(VerifyNewNativePageStartedContext, web_state, url, context, nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetPendingItem();
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// to native URLs passed to |DidFinishNavigation|. Asserts that
// |NavigationContext| the same as |context|.
ACTION_P4(VerifyNewNativePageFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for reload navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P4(VerifyReloadStartedContext, web_state, url, context, nav_id) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  *nav_id = (*context)->GetNavigationId();
  EXPECT_NE(0, *nav_id);
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_TRUE((*context)->IsRendererInitiated());
  EXPECT_FALSE((*context)->GetResponseHeaders());
  // TODO(crbug.com/676129): Reload does not create a pending item. Check
  // pending item once the bug is fixed. The slim navigation manager fixes this
  // bug.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    NavigationManager* navigation_manager = web_state->GetNavigationManager();
    NavigationItem* item = navigation_manager->GetPendingItem();
    EXPECT_EQ(url, item->GetURL());
  } else {
    EXPECT_FALSE(web_state->GetNavigationManager()->GetPendingItem());
  }
}

// Verifies correctness of |NavigationContext| (|arg1|) for reload navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifyReloadFinishedContext,
          web_state,
          url,
          context,
          nav_id,
          is_web_page) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->HasCommitted());
  EXPECT_FALSE((*context)->IsDownload());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_TRUE((*context)->IsRendererInitiated());
  if (is_web_page) {
    ASSERT_TRUE((*context)->GetResponseHeaders());
    std::string mime_type;
    (*context)->GetResponseHeaders()->GetMimeType(&mime_type);
    EXPECT_EQ(kExpectedMimeType, mime_type);
  } else {
    EXPECT_FALSE((*context)->GetResponseHeaders());
  }
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for download navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P4(VerifyDownloadFinishedContext, web_state, url, context, nav_id) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(*nav_id, (*context)->GetNavigationId());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->HasCommitted());
  EXPECT_TRUE((*context)->IsDownload());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
}

// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public WebStateObserver {
 public:
  WebStateObserverMock() = default;

  MOCK_METHOD2(DidStartNavigation, void(WebState*, NavigationContext*));
  MOCK_METHOD2(DidFinishNavigation, void(WebState*, NavigationContext*));
  MOCK_METHOD1(DidStartLoading, void(WebState*));
  MOCK_METHOD1(DidStopLoading, void(WebState*));
  MOCK_METHOD2(PageLoaded, void(WebState*, PageLoadCompletionStatus));
  void WebStateDestroyed(WebState* web_state) override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverMock);
};

// Mocks WebStatePolicyDecider decision callbacks.
class PolicyDeciderMock : public WebStatePolicyDecider {
 public:
  PolicyDeciderMock(WebState* web_state) : WebStatePolicyDecider(web_state) {}
  MOCK_METHOD2(ShouldAllowRequest, bool(NSURLRequest*, ui::PageTransition));
  MOCK_METHOD2(ShouldAllowResponse, bool(NSURLResponse*, bool for_main_frame));
};

// Responds with a page that contains an html form.
std::unique_ptr<net::test_server::HttpResponse> HandleFormPage(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/form") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("text/html");
    result->set_content(base::StringPrintf(
        "<form method='post' id='form' action='echo'>"
        "  <input type=''text' name='inttestname' value='%s'>"
        "</form>"
        "%s",
        kTestFormValue, kTestPageText));

    return std::move(result);
  }
  return nullptr;
}

// Responds with a download.
std::unique_ptr<net::test_server::HttpResponse> HandleDownloadPage(
    const net::test_server::HttpRequest& request) {
  if (request.GetURL().path() == "/download") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type(kDownloadMimeType);
    result->set_content(kTestPageText);
    return std::move(result);
  }
  return nullptr;
}

}  // namespace

using testing::Return;
using testing::StrictMock;
using testing::_;
using testing::WaitUntilConditionOrTimeout;
using web::test::WaitForWebViewContainingText;

// Test fixture for WebStateDelegate::ProvisionalNavigationStarted,
// WebStateDelegate::DidFinishNavigation, WebStateDelegate::DidStartLoading and
// WebStateDelegate::DidStopLoading integration tests.
class NavigationAndLoadCallbacksTest : public WebIntTest {
 public:
  NavigationAndLoadCallbacksTest() : scoped_observer_(&observer_) {}

  void SetUp() override {
    WebIntTest::SetUp();
    decider_ = std::make_unique<StrictMock<PolicyDeciderMock>>(web_state());
    scoped_observer_.Add(web_state());

    // Stub out NativeContent objects.
    provider_ = [[TestNativeContentProvider alloc] init];
    content_ = [[TestNativeContent alloc] initWithURL:GURL::EmptyGURL()
                                           virtualURL:GURL::EmptyGURL()];

    WebStateImpl* web_state_impl = reinterpret_cast<WebStateImpl*>(web_state());
    web_state_impl->GetWebController().nativeProvider = provider_;

    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    test_server_->RegisterDefaultHandler(base::BindRepeating(&HandleFormPage));
    test_server_->RegisterDefaultHandler(
        base::BindRepeating(&HandleDownloadPage));
    RegisterDefaultHandlers(test_server_.get());
    ASSERT_TRUE(test_server_->Start());
  }

  void TearDown() override {
    scoped_observer_.RemoveAll();
    WebIntTest::TearDown();
  }

 protected:
  TestNativeContentProvider* provider_;
  std::unique_ptr<StrictMock<PolicyDeciderMock>> decider_;
  TestNativeContent* content_;
  StrictMock<WebStateObserverMock> observer_;
  ScopedObserver<WebState, WebStateObserver> scoped_observer_;
  testing::InSequence callbacks_sequence_checker_;

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;

  DISALLOW_COPY_AND_ASSIGN(NavigationAndLoadCallbacksTest);
};

// Tests successful navigation to a new page.
TEST_F(NavigationAndLoadCallbacksTest, NewPageNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
}

// Tests that if web usage is already enabled, enabling it again would not cause
// any page loads (related to restoring cached session). This is a regression
// test for crbug.com/781916.
TEST_F(NavigationAndLoadCallbacksTest, EnableWebUsageTwice) {
  const GURL url = test_server_->GetURL("/echo");

  // Only expect one set of load events from the first LoadUrl(), not subsequent
  // SetWebUsageEnabled(true) calls. Web usage is already enabled, so the
  // subsequent calls should be noops.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  ASSERT_TRUE(LoadUrl(url));
  web_state()->SetWebUsageEnabled(true);
  web_state()->SetWebUsageEnabled(true);
}

// Tests failed navigation to a new page.
TEST_F(NavigationAndLoadCallbacksTest, FailedNavigation) {
  const GURL url = test_server_->GetURL("/close-socket");

  // Perform a navigation to url with unsupported scheme, which will fail.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(
          VerifyErrorFinishedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));
  // LoadUrl() expects sucessfull load and can not be used here.
  web::test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

// Tests web page reload navigation.
TEST_F(NavigationAndLoadCallbacksTest, WebPageReloadNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Reload web page.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(
          VerifyReloadStartedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context, &nav_id,
                                            true /* is_web_page */));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  // web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 false /*check_for_repost*/);
  }));
}

// Tests web page reload with user agent override.
TEST_F(NavigationAndLoadCallbacksTest, ReloadWithUserAgentType) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Reload web page with desktop user agent.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_RELOAD,
          &context, &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  // TODO(crbug.com/798836): verify the correct User-Agent header is sent.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);
  }));
}

// Tests user-initiated hash change.
TEST_F(NavigationAndLoadCallbacksTest, UserInitiatedHashChangeNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform same-document navigation.
  const GURL hash_url = test_server_->GetURL("/echo#1");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(hash_url));

  // Perform same-document navigation by going back.
  // No ShouldAllowRequest callback for same-document back-forward navigations.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callbacks for same-document back-forward
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  }));
}

// Tests renderer-initiated hash change.
TEST_F(NavigationAndLoadCallbacksTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = test_server_->GetURL("/echo#1");
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_F(NavigationAndLoadCallbacksTest, StateNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));

  // Perform push state using JavaScript.
  const GURL push_url = test_server_->GetURL("/test.html");
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), push_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for same-document push
  // state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), push_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = test_server_->GetURL("/1.html");
  // No ShouldAllowRequest callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), replace_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), replace_url, &context, &nav_id,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_F(NavigationAndLoadCallbacksTest, NativeContentNavigation) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageStartedContext(web_state(), url, &context,
                                                  &nav_id));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageFinishedContext(web_state(), url, &context,
                                                   &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  ASSERT_TRUE(LoadUrl(url));
}

// Tests native content reload navigation.
TEST_F(NavigationAndLoadCallbacksTest, NativeContentReload) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  ASSERT_TRUE(LoadUrl(url));

  // Reload native content.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  // No ShouldAllowRequest callbacks for native content navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(
          VerifyReloadStartedContext(web_state(), url, &context, &nav_id));
  // No ShouldAllowResponse callbacks for native content navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context, &nav_id,
                                            false /* is_web_page */));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  navigation_manager()->Reload(ReloadType::NORMAL, false /*check_for_repost*/);
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationAndLoadCallbacksTest, UserInitiatedPostNavigation) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), url, &context, &nav_id,
                                         /*renderer_initiated=*/false));
  if (@available(iOS 11, *)) {
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), url, &context, &nav_id,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  // Load request using POST HTTP method.
  web::NavigationManager::WebLoadParams params(url);
  params.post_data = [@"foo" dataUsingEncoding:NSUTF8StringEncoding];
  params.extra_headers = @{@"Content-Type" : @"text/html"};
  ASSERT_TRUE(LoadWithParams(params));
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), "foo"));
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationAndLoadCallbacksTest, RendererInitiatedPostNavigation) {
  const GURL url = test_server_->GetURL("/form");
  const GURL action = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context, &nav_id,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          &nav_id,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestFormValue));
}

// Tests successful reload of a page returned for post request.
TEST_F(NavigationAndLoadCallbacksTest, ReloadPostNavigation) {
  const GURL url = test_server_->GetURL("/form");
  const GURL action = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestFormValue));

  // Reload the page.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context, &nav_id,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          &nav_id,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 false /*check_for_repost*/);
  }));
}

// Tests going forward to a page rendered from post response.
TEST_F(NavigationAndLoadCallbacksTest, ForwardPostNavigation) {
  const GURL url = test_server_->GetURL("/form");
  const GURL action = test_server_->GetURL("/echo");

  // Perform new page navigation.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestFormValue));

  // Go Back.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  } else {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  }

  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _));
  if (@available(iOS 10, *)) {
    // Starting from iOS10, ShouldAllowResponse is not called when going back
    // after form submission.
  } else {
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  }));

  // Go forward.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  } else {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  }
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context, &nav_id,
                                         /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          &nav_id,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ASSERT_TRUE(ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->GoForward();
  }));
}

// Tests server redirect navigation.
TEST_F(NavigationAndLoadCallbacksTest, RedirectNavigation) {
  const GURL url = test_server_->GetURL("/server-redirect?echo");
  const GURL redirect_url = test_server_->GetURL("/echo");

  // Load url which replies with redirect.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  // Second ShouldAllowRequest call is for redirect_url.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(
          web_state(), redirect_url, kExpectedMimeType, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ASSERT_TRUE(LoadUrl(url));
}

// Tests download navigation.
TEST_F(NavigationAndLoadCallbacksTest, DownloadNavigation) {
  GURL url = test_server_->GetURL("/download");

  // Perform download navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(
          VerifyDownloadFinishedContext(web_state(), url, &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  web::test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForDownloadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

#if TARGET_IPHONE_SIMULATOR
#define MAYBE_FailedLoad FailedLoad
#else
#define MAYBE_FailedLoad DISABLED_FailedLoad
#endif
// Tests failed load after the navigation is sucessfully finished.
// TODO(crbug.com/812669): Re-enable this test on devices.
TEST_F(NavigationAndLoadCallbacksTest, MAYBE_FailedLoad) {
  GURL url = test_server_->GetURL("/exabyte_response");

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, /*mime_type=*/"",
                                             &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));
  web::test::LoadUrl(web_state(), url);

  // Server does not stop responding until it's shut down. Let the server run
  // to make web state finish the navigation.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(2.0, ^{
    return !web_state()->IsLoading();
  }));

  // It this point the navigation should be finished. Shutdown the server and
  // wait until web state stop loading.
  ASSERT_TRUE(test_server_->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

// Tests rejecting the navigation from ShouldAllowRequest. The load should stop,
// but no other callbacks are called.
TEST_F(NavigationAndLoadCallbacksTest, DisallowRequest) {
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  web::test::LoadUrl(web_state(), test_server_->GetURL("/echo"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

// Tests rejecting the navigation from ShouldAllowResponse. PageLoaded callback
// is not called.
TEST_F(NavigationAndLoadCallbacksTest, DisallowResponse) {
  const GURL url = test_server_->GetURL("/echo");

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(false));
  // TODO(crbug.com/809557): DidFinishNavigation should be called.
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  web::test::LoadUrl(web_state(), test_server_->GetURL("/echo"));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

// Tests stopping a navigation. Did FinishLoading and PageLoaded are never
// called.
TEST_F(NavigationAndLoadCallbacksTest, StopNavigation) {
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  web::test::LoadUrl(web_state(), test_server_->GetURL("/hung"));
  web_state()->Stop();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

#if TARGET_IPHONE_SIMULATOR
#define MAYBE_StopFinishedNavigation StopFinishedNavigation
#else
#define MAYBE_StopFinishedNavigation DISABLED_StopFinishedNavigation
#endif
// Tests stopping a finished navigation. PageLoaded is never called.
// TODO(crbug.com/812669): Re-enable this test on devices.
TEST_F(NavigationAndLoadCallbacksTest, MAYBE_StopFinishedNavigation) {
  GURL url = test_server_->GetURL("/exabyte_response");

  NavigationContext* context = nullptr;
  int32_t nav_id = 0;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPageStartedContext(
          web_state(), url, ui::PageTransition::PAGE_TRANSITION_TYPED, &context,
          &nav_id));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, /*mime_type=*/"",
                                             &context, &nav_id));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));

  web::test::LoadUrl(web_state(), url);

  // Server does not stop responding until it's shut down. Let the server run
  // to make web state finish the navigation.
  EXPECT_FALSE(WaitUntilConditionOrTimeout(2.0, ^{
    return !web_state()->IsLoading();
  }));

  // Stop the loading.
  web_state()->Stop();
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

}  // namespace web
