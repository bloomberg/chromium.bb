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
#import "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/html_response_provider_impl.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
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

const char kTestPageText[] = "landing!";
const char kExpectedMimeType[] = "text/html";

// Verifies correctness of |NavigationContext| (|arg1|) for new page navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P3(VerifyNewPageStartedContext, web_state, url, context) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P3(VerifyNewPageFinishedContext, web_state, url, context) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->IsRendererInitiated());
  ASSERT_TRUE((*context)->GetResponseHeaders());
  std::string mime_type;
  (*context)->GetResponseHeaders()->GetMimeType(&mime_type);
  ASSERT_TRUE(web_state->IsLoading());
  EXPECT_EQ(kExpectedMimeType, mime_type);
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationItem* item = navigation_manager->GetLastCommittedItem();
  EXPECT_TRUE(!item->GetTimestamp().is_null());
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg1|) for failed navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P3(VerifyErrorFinishedContext, web_state, url, context) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->IsPost());
  // The error code will be different on bots and for local runs. Allow both.
  NSInteger error_code = (*context)->GetError().code;
  EXPECT_TRUE(error_code == NSURLErrorCannotFindHost ||
              error_code == NSURLErrorNotConnectedToInternet);
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
ACTION_P4(VerifyPostStartedContext,
          web_state,
          url,
          context,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P4(VerifyPostFinishedContext,
          web_state,
          url,
          context,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  ASSERT_TRUE((*context));
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P5(VerifySameDocumentStartedContext,
          web_state,
          url,
          context,
          page_transition,
          renderer_initiated) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
}

// Verifies correctness of |NavigationContext| (|arg1|) for same page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifySameDocumentFinishedContext,
          web_state,
          url,
          context,
          page_transition,
          renderer_initiated) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
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
ACTION_P3(VerifyNewNativePageStartedContext, web_state, url, context) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P3(VerifyNewNativePageFinishedContext, web_state, url, context) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_TYPED,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P3(VerifyReloadStartedContext, web_state, url, context) {
  *context = arg1;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
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
ACTION_P4(VerifyReloadFinishedContext, web_state, url, context, is_web_page) {
  ASSERT_EQ(*context, arg1);
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(
      PageTransitionCoreTypeIs(ui::PageTransition::PAGE_TRANSITION_RELOAD,
                               (*context)->GetPageTransition()));
  EXPECT_FALSE((*context)->IsSameDocument());
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

}  // namespace

using test::HttpServer;
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

  DISALLOW_COPY_AND_ASSIGN(NavigationAndLoadCallbacksTest);
};

// Tests successful navigation to a new page.
TEST_F(NavigationAndLoadCallbacksTest, NewPageNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(url);
}

// Tests that if web usage is already enabled, enabling it again would not cause
// any page loads (related to restoring cached session). This is a regression
// test for crbug.com/781916.
TEST_F(NavigationAndLoadCallbacksTest, EnableWebUsageTwice) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Only expect one set of load events from the first LoadUrl(), not subsequent
  // SetWebUsageEnabled(true) calls. Web usage is already enabled, so the
  // subsequent calls should be noops.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  LoadUrl(url);
  web_state()->SetWebUsageEnabled(true);
  web_state()->SetWebUsageEnabled(true);
}

// Tests failed navigation to a new page.
TEST_F(NavigationAndLoadCallbacksTest, FailedNavigation) {
  const GURL url = HttpServer::MakeUrl("unsupported://chromium.test");

  // Perform a navigation to url with unsupported scheme, which will fail.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  // TODO(crbug.com/803232): PageLoaded() should be called after
  // DidFinishNavigation().
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::FAILURE));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyErrorFinishedContext(web_state(), url, &context));
  // LoadUrl() expects sucessfull load and can not be used here.
  web::test::LoadUrl(web_state(), url);
  EXPECT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForPageLoadTimeout, ^{
    return !web_state()->IsLoading();
  }));
}

// Tests web page reload navigation.
TEST_F(NavigationAndLoadCallbacksTest, WebPageReloadNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

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
  LoadUrl(url);

  // Reload web page.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyReloadStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context,
                                            true /* is_web_page */));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 false /*check_for_repost*/);
  });
}

// Tests user-initiated hash change.
TEST_F(NavigationAndLoadCallbacksTest, UserInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(url);

  // Perform same-document navigation.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(hash_url);

  // Perform same-document navigation by going back.
  // No ShouldAllowRequest callback for same-document back-forward navigations.
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), url, &context,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callbacks for same-document back-forward
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), url, &context,
          ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });
}

// Tests renderer-initiated hash change.
TEST_F(NavigationAndLoadCallbacksTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(url);

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_F(NavigationAndLoadCallbacksTest, StateNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(url);

  // Perform push state using JavaScript.
  const GURL push_url = HttpServer::MakeUrl("http://chromium.test/test.html");
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), push_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for same-document push
  // state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), push_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = HttpServer::MakeUrl("http://chromium.test/1.html");
  // No ShouldAllowRequest callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), replace_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callbacks for same-document push state navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), replace_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_F(NavigationAndLoadCallbacksTest, NativeContentNavigation) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageStartedContext(web_state(), url, &context));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyNewNativePageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  [provider_ setController:content_ forURL:url];
  LoadUrl(url);
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
  LoadUrl(url);

  // Reload native content.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  // No ShouldAllowRequest callbacks for native content navigations.
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyReloadStartedContext(web_state(), url, &context));
  // No ShouldAllowResponse callbacks for native content navigations.
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context,
                                            false /* is_web_page */));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  navigation_manager()->Reload(ReloadType::NORMAL, false /*check_for_repost*/);
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationAndLoadCallbacksTest, UserInitiatedPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = kTestPageText;
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), url, &context,
                                         /*renderer_initiated=*/false));
  if (@available(iOS 11, *)) {
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
  }
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), url, &context,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));

  // Load request using POST HTTP method.
  web::NavigationManager::WebLoadParams params(url);
  params.post_data = [@"foo" dataUsingEncoding:NSUTF8StringEncoding];
  params.extra_headers = @{@"Content-Type" : @"text/html"};
  LoadWithParams(params);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationAndLoadCallbacksTest, RendererInitiatedPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  const GURL action = HttpServer::MakeUrl("http://action.test");
  std::map<GURL, std::string> responses;
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

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
  LoadUrl(url);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));
}

// Tests successful reload of a page returned for post request.
TEST_F(NavigationAndLoadCallbacksTest, ReloadPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  const GURL action = HttpServer::MakeUrl("http://action.test");
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

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
  LoadUrl(url);
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
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));

  // Reload the page.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->Reload(ReloadType::NORMAL,
                                 false /*check_for_repost*/);
  });
}

// Tests going forward to a page rendered from post response.
TEST_F(NavigationAndLoadCallbacksTest, ForwardPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  const GURL action = HttpServer::MakeUrl("http://action.test");
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

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
  LoadUrl(url);
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
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));

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
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });

  // Go forward.
  NavigationContext* context = nullptr;
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
  } else {
    EXPECT_CALL(observer_, DidStartLoading(web_state()));
    EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  }
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->GoForward();
  });
}

// Tests server redirect navigation.
TEST_F(NavigationAndLoadCallbacksTest, RedirectNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, HtmlResponseProviderImpl::Response> responses;
  const GURL redirect_url = HttpServer::MakeUrl("http://chromium2.test");
  responses[url] = HtmlResponseProviderImpl::GetRedirectResponse(
      redirect_url, net::HTTP_MOVED_PERMANENTLY);
  responses[redirect_url] = HtmlResponseProviderImpl::GetSimpleResponse("here");
  std::unique_ptr<web::DataResponseProvider> provider(
      new HtmlResponseProvider(responses));
  web::test::SetUpHttpServer(std::move(provider));

  // Load url which replies with redirect.
  NavigationContext* context = nullptr;
  EXPECT_CALL(observer_, DidStartLoading(web_state()));
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(observer_, DidStartNavigation(web_state(), _))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  // Second ShouldAllowRequest call is for redirect_url.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(observer_, DidFinishNavigation(web_state(), _))
      .WillOnce(
          VerifyNewPageFinishedContext(web_state(), redirect_url, &context));
  EXPECT_CALL(observer_, DidStopLoading(web_state()));
  EXPECT_CALL(observer_,
              PageLoaded(web_state(), PageLoadCompletionStatus::SUCCESS));
  LoadUrl(url);
}

}  // namespace web
