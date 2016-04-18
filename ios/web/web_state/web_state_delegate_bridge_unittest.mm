// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state_delegate_bridge.h"

#include <memory>

#import "base/mac/scoped_nsobject.h"
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

}  // namespace web
