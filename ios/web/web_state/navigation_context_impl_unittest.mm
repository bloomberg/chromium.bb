// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/navigation_context_impl.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
const char kRawResponseHeaders[] =
    "HTTP/1.1 200 OK\0"
    "Content-Length: 450\0"
    "Connection: keep-alive\0";
}  // namespace

// Test fixture for NavigationContextImplTest testing.
class NavigationContextImplTest : public PlatformTest {
 protected:
  NavigationContextImplTest()
      : url_("https://chromium.test"),
        response_headers_(new net::HttpResponseHeaders(
            std::string(kRawResponseHeaders, sizeof(kRawResponseHeaders)))) {}

  TestWebState web_state_;
  GURL url_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

// Tests CreateNavigationContext factory method.
TEST_F(NavigationContextImplTest, NavigationContext) {
  std::unique_ptr<NavigationContext> context =
      NavigationContextImpl::CreateNavigationContext(
          &web_state_, url_, ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          true);
  ASSERT_TRUE(context);

  EXPECT_EQ(&web_state_, context->GetWebState());
  EXPECT_TRUE(PageTransitionTypeIncludingQualifiersIs(
      context->GetPageTransition(),
      ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK));
  EXPECT_EQ(url_, context->GetUrl());
  EXPECT_FALSE(context->IsSameDocument());
  EXPECT_FALSE(context->GetError());
  EXPECT_FALSE(context->GetResponseHeaders());
  EXPECT_TRUE(context->IsRendererInitiated());
}

// Tests NavigationContextImpl Setters.
TEST_F(NavigationContextImplTest, Setters) {
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          &web_state_, url_, ui::PageTransition::PAGE_TRANSITION_FORWARD_BACK,
          false);
  ASSERT_TRUE(context);

  ASSERT_FALSE(context->IsSameDocument());
  ASSERT_FALSE(context->IsPost());
  ASSERT_FALSE(context->GetError());
  ASSERT_FALSE(context->IsRendererInitiated());
  ASSERT_NE(response_headers_.get(), context->GetResponseHeaders());

  // SetSameDocument
  context->SetIsSameDocument(true);
  EXPECT_TRUE(context->IsSameDocument());
  ASSERT_FALSE(context->IsPost());
  EXPECT_FALSE(context->GetError());
  EXPECT_FALSE(context->IsRendererInitiated());
  EXPECT_NE(response_headers_.get(), context->GetResponseHeaders());

  // SetPost
  context->SetIsPost(true);
  EXPECT_TRUE(context->IsSameDocument());
  ASSERT_TRUE(context->IsPost());
  EXPECT_FALSE(context->GetError());
  EXPECT_FALSE(context->IsRendererInitiated());
  EXPECT_NE(response_headers_.get(), context->GetResponseHeaders());

  // SetErrorPage
  NSError* error = [[NSError alloc] init];
  context->SetError(error);
  EXPECT_TRUE(context->IsSameDocument());
  ASSERT_TRUE(context->IsPost());
  EXPECT_EQ(error, context->GetError());
  EXPECT_FALSE(context->IsRendererInitiated());
  EXPECT_NE(response_headers_.get(), context->GetResponseHeaders());

  // SetResponseHeaders
  context->SetResponseHeaders(response_headers_);
  EXPECT_TRUE(context->IsSameDocument());
  ASSERT_TRUE(context->IsPost());
  EXPECT_EQ(error, context->GetError());
  EXPECT_FALSE(context->IsRendererInitiated());
  EXPECT_EQ(response_headers_.get(), context->GetResponseHeaders());

  // SetIsRendererInitiated
  context->SetIsRendererInitiated(true);
  EXPECT_TRUE(context->IsSameDocument());
  ASSERT_TRUE(context->IsPost());
  EXPECT_EQ(error, context->GetError());
  EXPECT_TRUE(context->IsRendererInitiated());
  EXPECT_EQ(response_headers_.get(), context->GetResponseHeaders());
}

}  // namespace web
