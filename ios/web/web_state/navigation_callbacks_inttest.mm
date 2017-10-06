// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/test/fakes/test_native_content.h"
#import "ios/web/public/test/fakes/test_native_content_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
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

// Verifies correctness of |NavigationContext| (|arg0|) for new page navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P3(VerifyNewPageStartedContext, web_state, url, context) {
  *context = arg0;
  ASSERT_TRUE(*context);
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

// Verifies correctness of |NavigationContext| (|arg0|) for new page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P3(VerifyNewPageFinishedContext, web_state, url, context) {
  ASSERT_EQ(*context, arg0);
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
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg0|) for navigations via POST
// HTTP methods passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P4(VerifyPostStartedContext,
          web_state,
          url,
          context,
          renderer_initiated) {
  *context = arg0;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_FALSE((*context)->IsSameDocument());
  EXPECT_TRUE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_EQ(renderer_initiated, (*context)->IsRendererInitiated());
  ASSERT_FALSE((*context)->GetResponseHeaders());
  ASSERT_TRUE(web_state->IsLoading());
  // TODO(crbug.com/676129): Reload does not create a pending item. Remove this
  // workaround once the bug is fixed.
  if (!ui::PageTransitionTypeIncludingQualifiersIs(
          ui::PageTransition::PAGE_TRANSITION_RELOAD,
          (*context)->GetPageTransition())) {
    NavigationManager* navigation_manager = web_state->GetNavigationManager();
    NavigationItem* item = navigation_manager->GetPendingItem();
    EXPECT_EQ(url, item->GetURL());
  }
}

// Verifies correctness of |NavigationContext| (|arg0|) for navigations via POST
// HTTP methods passed to |DidFinishNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P4(VerifyPostFinishedContext,
          web_state,
          url,
          context,
          renderer_initiated) {
  ASSERT_EQ(*context, arg0);
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
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg0|) for same page navigation
// passed to |DidFinishNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P5(VerifySameDocumentStartedContext,
          web_state,
          url,
          context,
          page_transition,
          renderer_initiated) {
  *context = arg0;
  ASSERT_TRUE(*context);
  EXPECT_EQ(web_state, (*context)->GetWebState());
  EXPECT_EQ(url, (*context)->GetUrl());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      page_transition, (*context)->GetPageTransition()));
  EXPECT_TRUE((*context)->IsSameDocument());
  EXPECT_FALSE((*context)->IsPost());
  EXPECT_FALSE((*context)->GetError());
  EXPECT_FALSE((*context)->GetResponseHeaders());
}

// Verifies correctness of |NavigationContext| (|arg0|) for same page navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P5(VerifySameDocumentFinishedContext,
          web_state,
          url,
          context,
          page_transition,
          renderer_initiated) {
  ASSERT_EQ(*context, arg0);
  ASSERT_TRUE(*context);
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
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg0|) for new page navigation
// to native URLs passed to |DidStartNavigation|. Stores |NavigationContext| in
// |context| pointer.
ACTION_P3(VerifyNewNativePageStartedContext, web_state, url, context) {
  *context = arg0;
  ASSERT_TRUE(*context);
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

// Verifies correctness of |NavigationContext| (|arg0|) for new page navigation
// to native URLs passed to |DidFinishNavigation|. Asserts that
// |NavigationContext| the same as |context|.
ACTION_P3(VerifyNewNativePageFinishedContext, web_state, url, context) {
  ASSERT_EQ(*context, arg0);
  ASSERT_TRUE(*context);
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
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Verifies correctness of |NavigationContext| (|arg0|) for reload navigation
// passed to |DidStartNavigation|. Stores |NavigationContext| in |context|
// pointer.
ACTION_P3(VerifyReloadStartedContext, web_state, url, context) {
  *context = arg0;
  ASSERT_TRUE(*context);
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
  // pending item once the bug is fixed.
  EXPECT_FALSE(web_state->GetNavigationManager()->GetPendingItem());
}

// Verifies correctness of |NavigationContext| (|arg0|) for reload navigation
// passed to |DidFinishNavigation|. Asserts that |NavigationContext| the same as
// |context|.
ACTION_P4(VerifyReloadFinishedContext, web_state, url, context, is_web_page) {
  ASSERT_EQ(*context, arg0);
  ASSERT_TRUE(*context);
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
  EXPECT_GT(item->GetTimestamp().ToInternalValue(), 0);
  EXPECT_EQ(url, item->GetURL());
}

// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public WebStateObserver {
 public:
  WebStateObserverMock(WebState* web_state) : WebStateObserver(web_state) {}
  MOCK_METHOD1(DidStartNavigation, void(NavigationContext* context));
  MOCK_METHOD1(DidFinishNavigation, void(NavigationContext* context));
  MOCK_METHOD0(DidStartLoading, void());
  MOCK_METHOD0(DidStopLoading, void());
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
using web::test::WaitForWebViewContainingText;

// Test fixture for WebStateDelegate::ProvisionalNavigationStarted,
// WebStateDelegate::DidFinishNavigation, WebStateDelegate::DidStartLoading and
// WebStateDelegate::DidStopLoading integration tests.
class NavigationCallbacksTest : public WebIntTest {
  void SetUp() override {
    WebIntTest::SetUp();
    observer_ = base::MakeUnique<StrictMock<WebStateObserverMock>>(web_state());
    decider_ = base::MakeUnique<StrictMock<PolicyDeciderMock>>(web_state());

    // Stub out NativeContent objects.
    provider_.reset([[TestNativeContentProvider alloc] init]);
    content_.reset([[TestNativeContent alloc] initWithURL:GURL::EmptyGURL()
                                               virtualURL:GURL::EmptyGURL()]);

    WebStateImpl* web_state_impl = reinterpret_cast<WebStateImpl*>(web_state());
    web_state_impl->GetWebController().nativeProvider = provider_.get();
  }

 protected:
  base::scoped_nsobject<TestNativeContentProvider> provider_;
  std::unique_ptr<StrictMock<PolicyDeciderMock>> decider_;
  base::scoped_nsobject<TestNativeContent> content_;
  std::unique_ptr<StrictMock<WebStateObserverMock>> observer_;
  testing::InSequence callbacks_sequence_checker_;
};

// Tests successful navigation to a new page.
TEST_F(NavigationCallbacksTest, NewPageNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);
}

// Tests web page reload navigation.
TEST_F(NavigationCallbacksTest, WebPageReloadNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);

  // Reload web page.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyReloadStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context,
                                            true /* is_web_page */));
  EXPECT_CALL(*observer_, DidStopLoading());
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
TEST_F(NavigationCallbacksTest, UserInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);

  // Perform same-document navigation.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_TYPED,
          /*renderer_initiated=*/false));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(hash_url);

  // Perform same-document navigation by going back.
  // No ShouldAllowRequest callback for same-document back-forward navigations.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/false));
  // No ShouldAllowResponse callbacks for same-document back-forward
  // navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/false));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });
}

// Tests renderer-initiated hash change.
TEST_F(NavigationCallbacksTest, RendererInitiatedHashChangeNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);

  // Perform same-page navigation using JavaScript.
  const GURL hash_url = HttpServer::MakeUrl("http://chromium.test#1");
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callback for same-document navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), hash_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteJavaScript(@"window.location.hash = '#1'");
}

// Tests state change.
TEST_F(NavigationCallbacksTest, StateNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = "Chromium Test";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyNewPageStartedContext(web_state(), url, &context));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewPageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);

  // Perform push state using JavaScript.
  const GURL push_url = HttpServer::MakeUrl("http://chromium.test/test.html");
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), push_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for same-document push
  // state navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), push_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.pushState('', 'Test', 'test.html')");

  // Perform replace state using JavaScript.
  const GURL replace_url = HttpServer::MakeUrl("http://chromium.test/1.html");
  // No ShouldAllowRequest callbacks for same-document push state navigations.
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifySameDocumentStartedContext(
          web_state(), replace_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  // No ShouldAllowResponse callbacks for same-document push state navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifySameDocumentFinishedContext(
          web_state(), replace_url, &context,
          ui::PageTransition::PAGE_TRANSITION_CLIENT_REDIRECT,
          /*renderer_initiated=*/true));
  ExecuteJavaScript(@"window.history.replaceState('', 'Test', '1.html')");
}

// Tests native content navigation.
TEST_F(NavigationCallbacksTest, NativeContentNavigation) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyNewNativePageStartedContext(web_state(), url, &context));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyNewNativePageFinishedContext(web_state(), url, &context));
  EXPECT_CALL(*observer_, DidStopLoading());
  [provider_ setController:content_.get() forURL:url];
  LoadUrl(url);
}

// Tests native content reload navigation.
TEST_F(NavigationCallbacksTest, NativeContentReload) {
  GURL url(url::SchemeHostPort(kTestNativeContentScheme, "ui", 0).Serialize());
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  // No ShouldAllowRequest/ShouldAllowResponse callbacks for native content
  // navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  [provider_ setController:content_.get() forURL:url];
  LoadUrl(url);

  // Reload native content.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  // No ShouldAllowRequest callbacks for native content navigations.
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyReloadStartedContext(web_state(), url, &context));
  // No ShouldAllowResponse callbacks for native content navigations.
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyReloadFinishedContext(web_state(), url, &context,
                                            false /* is_web_page */));
  EXPECT_CALL(*observer_, DidStopLoading());
  navigation_manager()->Reload(ReloadType::NORMAL, false /*check_for_repost*/);
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationCallbacksTest, UserInitiatedPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  responses[url] = kTestPageText;
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyPostStartedContext(web_state(), url, &context,
                                         /*renderer_initiated=*/false));
  if (@available(iOS 11, *)) {
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
  }
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyPostFinishedContext(web_state(), url, &context,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(*observer_, DidStopLoading());

  // Load request using POST HTTP method.
  web::NavigationManager::WebLoadParams params(url);
  params.post_data.reset([@"foo" dataUsingEncoding:NSUTF8StringEncoding]);
  params.extra_headers.reset(@{ @"Content-Type" : @"text/html" });
  LoadWithParams(params);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));
}

// Tests successful navigation to a new page with post HTTP method.
TEST_F(NavigationCallbacksTest, RendererInitiatedPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  const GURL action = HttpServer::MakeUrl("http://action.test");
  std::map<GURL, std::string> responses;
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteJavaScript(@"document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));
}

// Tests successful reload of a page returned for post request.
TEST_F(NavigationCallbacksTest, ReloadPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  const GURL action = HttpServer::MakeUrl("http://action.test");
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));

  // Reload the page.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/true));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/true));
  EXPECT_CALL(*observer_, DidStopLoading());
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
TEST_F(NavigationCallbacksTest, ForwardPostNavigation) {
  const GURL url = HttpServer::MakeUrl("http://chromium.test");
  std::map<GURL, std::string> responses;
  const GURL action = HttpServer::MakeUrl("http://action.test");
  responses[url] =
      base::StringPrintf("<form method='post' id='form' action='%s'></form>%s",
                         action.spec().c_str(), kTestPageText);
  responses[action] = "arrived!";
  web::test::SetUpSimpleHttpServer(responses);

  // Perform new page navigation.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  LoadUrl(url);
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), kTestPageText));

  // Submit the form using JavaScript.
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
      .WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteJavaScript(@"window.document.getElementById('form').submit();");
  ASSERT_TRUE(WaitForWebViewContainingText(web_state(), responses[action]));

  // Go Back.
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_));
  if (@available(iOS 10, *)) {
    // Starting from iOS10, ShouldAllowResponse is not called when going back
    // after form submission.
  } else {
    EXPECT_CALL(*decider_, ShouldAllowResponse(_, /*for_main_frame=*/true))
        .WillOnce(Return(true));
  }
  EXPECT_CALL(*observer_, DidFinishNavigation(_));
  EXPECT_CALL(*observer_, DidStopLoading());
  ExecuteBlockAndWaitForLoad(url, ^{
    navigation_manager()->GoBack();
  });

  // Go forward.
  NavigationContext* context = nullptr;
  EXPECT_CALL(*observer_, DidStartLoading());
  EXPECT_CALL(*decider_, ShouldAllowRequest(_, _)).WillOnce(Return(true));
  EXPECT_CALL(*observer_, DidStartNavigation(_))
      .WillOnce(VerifyPostStartedContext(web_state(), action, &context,
                                         /*renderer_initiated=*/false));
  EXPECT_CALL(*observer_, DidFinishNavigation(_))
      .WillOnce(VerifyPostFinishedContext(web_state(), action, &context,
                                          /*renderer_initiated=*/false));
  EXPECT_CALL(*observer_, DidStopLoading());
  // TODO(crbug.com/700958): ios/web ignores |check_for_repost| flag and current
  // delegate does not run callback for ShowRepostFormWarningDialog. Clearing
  // the delegate will allow form resubmission. Remove this workaround (clearing
  // the delegate, once |check_for_repost| is supported).
  web_state()->SetDelegate(nullptr);
  ExecuteBlockAndWaitForLoad(action, ^{
    navigation_manager()->GoForward();
  });
}

}  // namespace web
