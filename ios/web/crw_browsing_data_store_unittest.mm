// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_browsing_data_store.h"

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class BrowsingDataStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    browser_state_.reset(new TestBrowserState());
    BrowserState::GetActiveStateManager(browser_state_.get())->SetActive(true);
    browsing_data_store_.reset([[CRWBrowsingDataStore alloc]
        initWithBrowserState:browser_state_.get()]);
  }
  void TearDown() override {
    // The BrowserState needs to be destroyed first so that it is outlived by
    // the WebThreadBundle.
    BrowserState::GetActiveStateManager(browser_state_.get())->SetActive(false);
    browser_state_.reset();
    PlatformTest::TearDown();
  }

  // The CRWBrowsingDataStore used for testing purposes.
  base::scoped_nsobject<CRWBrowsingDataStore> browsing_data_store_;

 private:
  // The WebThreadBundle used for testing purposes.
  TestWebThreadBundle thread_bundle_;
  // The BrowserState used for testing purposes.
  scoped_ptr<BrowserState> browser_state_;
};

}  // namespace

// Tests that a CRWBrowsingDataStore's initial mode is set correctly and that it
// has no pending operations.
TEST_F(BrowsingDataStoreTest, InitialModeAndNoPendingOperations) {
  EXPECT_EQ(ACTIVE, [browsing_data_store_ mode]);
  EXPECT_FALSE([browsing_data_store_ hasPendingOperations]);
}

// Tests that CRWBrowsingDataStore handles several consecutive calls to
// |makeActive| and |makeInactive| correctly.
TEST_F(BrowsingDataStoreTest, MakeActiveAndInactiveOperations) {
  ProceduralBlock makeActiveCallback = ^{
    ASSERT_TRUE([NSThread isMainThread]);
    CRWBrowsingDataStoreMode mode = [browsing_data_store_ mode];
    EXPECT_TRUE((mode == ACTIVE) || (mode == SYNCHRONIZING));
  };
  ProceduralBlock makeInactiveCallback = ^{
    ASSERT_TRUE([NSThread isMainThread]);
    CRWBrowsingDataStoreMode mode = [browsing_data_store_ mode];
    EXPECT_TRUE((mode == INACTIVE) || (mode == SYNCHRONIZING));
  };
  [browsing_data_store_ makeActiveWithCompletionHandler:makeActiveCallback];
  EXPECT_EQ(SYNCHRONIZING, [browsing_data_store_ mode]);

  [browsing_data_store_ makeInactiveWithCompletionHandler:makeInactiveCallback];
  EXPECT_EQ(SYNCHRONIZING, [browsing_data_store_ mode]);

  [browsing_data_store_ makeActiveWithCompletionHandler:makeActiveCallback];
  EXPECT_EQ(SYNCHRONIZING, [browsing_data_store_ mode]);

  __block BOOL block_was_called = NO;
  [browsing_data_store_ makeInactiveWithCompletionHandler:^{
    makeInactiveCallback();
    block_was_called = YES;
  }];
  EXPECT_EQ(SYNCHRONIZING, [browsing_data_store_ mode]);

  base::test::ios::WaitUntilCondition(^bool{
    return block_was_called;
  });
}

// Tests that CRWBrowsingDataStore correctly handles |removeDataOfTypes:| call.
TEST_F(BrowsingDataStoreTest, RemoveDataOperations) {
  NSSet* browsing_data_types =
      [NSSet setWithObject:web::kBrowsingDataTypeCookies];
  __block BOOL block_was_called = NO;
  [browsing_data_store_ removeDataOfTypes:browsing_data_types
                        completionHandler:^{
                          DCHECK([NSThread isMainThread]);
                          block_was_called = YES;
                        }];
  base::test::ios::WaitUntilCondition(^bool() {
    return block_was_called;
  });
}

}  // namespace web
