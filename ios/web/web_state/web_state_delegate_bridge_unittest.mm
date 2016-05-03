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
  web::ContextMenuParams contextMenuParams;
  contextMenuParams.menu_title = base::UTF8ToUTF16("Menu title");
  contextMenuParams.link_url = GURL("http://www.url.com");
  contextMenuParams.src_url = GURL("http://www.url.com/image.jpeg");
  contextMenuParams.referrer_policy = web::ReferrerPolicyOrigin;
  contextMenuParams.view = [UIView new];
  contextMenuParams.location = CGPointMake(5.0, 5.0);
  bridge_->HandleContextMenu(nullptr, contextMenuParams);
  web::ContextMenuParams* resultParams = [delegate_ contextMenuParams];
  EXPECT_NE(nullptr, resultParams);
  EXPECT_EQ(contextMenuParams.menu_title, resultParams->menu_title);
  EXPECT_EQ(contextMenuParams.link_url, resultParams->link_url);
  EXPECT_EQ(contextMenuParams.src_url, resultParams->src_url);
  EXPECT_EQ(contextMenuParams.referrer_policy, resultParams->referrer_policy);
  EXPECT_EQ(contextMenuParams.view, resultParams->view);
  EXPECT_EQ(contextMenuParams.location.x, resultParams->location.x);
  EXPECT_EQ(contextMenuParams.location.y, resultParams->location.y);
}

}  // namespace web
