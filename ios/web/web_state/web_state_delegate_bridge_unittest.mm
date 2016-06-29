// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/web_state/web_state_delegate_stub.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/gtest_support.h"

namespace web {

// Test fixture to test WebStateDelegateBridge class.
class WebStateDelegateBridgeTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    id originalMockDelegate =
        [OCMockObject niceMockForProtocol:@protocol(CRWWebStateDelegate)];
    delegate_.reset([[CRWWebStateDelegateStub alloc]
        initWithRepresentedObject:originalMockDelegate]);

    bridge_.reset(new WebStateDelegateBridge(delegate_.get()));
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(delegate_);
    PlatformTest::TearDown();
  }

  base::scoped_nsprotocol<id> delegate_;
  std::unique_ptr<WebStateDelegateBridge> bridge_;
};

// Tests |LoadProgressChanged| forwarding.
TEST_F(WebStateDelegateBridgeTest, LoadProgressChanged) {
  ASSERT_EQ(0.0, [delegate_ changedProgress]);
  bridge_->LoadProgressChanged(nullptr, 1.0);
  EXPECT_EQ(1.0, [delegate_ changedProgress]);
}

// Tests |HandleContextMenu| forwarding.
TEST_F(WebStateDelegateBridgeTest, HandleContextMenu) {
  EXPECT_EQ(nil, [delegate_ contextMenuParams]);
  web::ContextMenuParams context_menu_params;
  context_menu_params.menu_title.reset([@"Menu title" copy]);
  context_menu_params.link_url = GURL("http://www.url.com");
  context_menu_params.src_url = GURL("http://www.url.com/image.jpeg");
  context_menu_params.referrer_policy = web::ReferrerPolicyOrigin;
  context_menu_params.view.reset([[UIView alloc] init]);
  context_menu_params.location = CGPointMake(5.0, 5.0);
  bridge_->HandleContextMenu(nullptr, context_menu_params);
  web::ContextMenuParams* result_params = [delegate_ contextMenuParams];
  EXPECT_NE(nullptr, result_params);
  EXPECT_EQ(context_menu_params.menu_title, result_params->menu_title);
  EXPECT_EQ(context_menu_params.link_url, result_params->link_url);
  EXPECT_EQ(context_menu_params.src_url, result_params->src_url);
  EXPECT_EQ(context_menu_params.referrer_policy,
            result_params->referrer_policy);
  EXPECT_EQ(context_menu_params.view, result_params->view);
  EXPECT_EQ(context_menu_params.location.x, result_params->location.x);
  EXPECT_EQ(context_menu_params.location.y, result_params->location.y);
}

// Tests |GetJavaScriptDialogPresenter| forwarding.
TEST_F(WebStateDelegateBridgeTest, GetJavaScriptDialogPresenter) {
  EXPECT_FALSE([delegate_ javaScriptDialogPresenterRequested]);
  bridge_->GetJavaScriptDialogPresenter(nullptr);
  EXPECT_TRUE([delegate_ javaScriptDialogPresenterRequested]);
}

}  // namespace web
