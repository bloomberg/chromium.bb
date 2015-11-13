// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_browsing_data_store.h"

#include "base/ios/ios_util.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/test/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// An observer to observe the |mode| key changes to a CRWBrowsingDataStore.
// Used for testing purposes.
@interface CRWTestBrowsingDataStoreObserver : NSObject
// The number of times that the mode of the underlying CRWBrowsingDataStore
// changed.
@property(nonatomic, assign) NSUInteger modeChangeCount;

// |browsingDataStore| cannot be null.
- (instancetype)initWithBrowsingDataStore:
    (CRWBrowsingDataStore*)browsingDataStore NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@end

@implementation CRWTestBrowsingDataStoreObserver {
  // The underlying CRWBrowsingDataStore.
  CRWBrowsingDataStore* _browsingDataStore;  // weak
}

@synthesize modeChangeCount = _modeChangeCount;

- (instancetype)initWithBrowsingDataStore:
    (CRWBrowsingDataStore*)browsingDataStore {
  self = [super init];
  if (self) {
    DCHECK(browsingDataStore);
    [browsingDataStore addObserver:self
                        forKeyPath:@"mode"
                           options:0
                           context:nil];
    _browsingDataStore = browsingDataStore;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  [_browsingDataStore removeObserver:self forKeyPath:@"mode"];
  [super dealloc];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  DCHECK([keyPath isEqual:@"mode"]);
  DCHECK_EQ(_browsingDataStore, object);

  ++self.modeChangeCount;
}

@end

namespace web {
namespace {

// A test fixture for testing CRWBrowsingDataStore.
class BrowsingDataStoreTest : public WebTest {
 protected:
  void SetUp() override {
    WebTest::SetUp();
    ASSERT_TRUE(
        BrowserState::GetActiveStateManager(GetBrowserState())->IsActive());
    browsing_data_store_.reset(
        [[CRWBrowsingDataStore alloc] initWithBrowserState:GetBrowserState()]);
  }

  // Sets the mode of the |browsing_data_store_| to |ACTIVE| and blocks until
  // the mode has actually been changed.
  void MakeActive() {
    [browsing_data_store_ makeActiveWithCompletionHandler:^(BOOL success) {
      DCHECK(success);
    }];
    base::test::ios::WaitUntilCondition(^bool() {
      return [browsing_data_store_ mode] == ACTIVE;
    });
  }

  // Sets the mode of the |browsing_data_store_| to |INACTIVE| and blocks until
  // the mode has actually been changed.
  void MakeInactive() {
    [browsing_data_store_ makeInactiveWithCompletionHandler:^(BOOL success) {
      DCHECK(success);
    }];
    base::test::ios::WaitUntilCondition(^bool() {
      return [browsing_data_store_ mode] == INACTIVE;
    });
  }

  // Removes browsing data of |browsingDataTypes| from the underlying
  // CRWBrowsingDataStore and waits until the operation finished.
  void RemoveDataOfTypes(web::BrowsingDataTypes browsing_data_types) {
    __block BOOL block_was_called = NO;
    [browsing_data_store_ removeDataOfTypes:browsing_data_types
                          completionHandler:^{
                            block_was_called = YES;
                          }];
    base::test::ios::WaitUntilCondition(^bool() {
      return block_was_called;
    });
  }

  // The CRWBrowsingDataStore used for testing purposes.
  base::scoped_nsobject<CRWBrowsingDataStore> browsing_data_store_;
};

}  // namespace

// Tests that a CRWBrowsingDataStore's initial mode is set correctly and that it
// has no pending operations.
TEST_F(BrowsingDataStoreTest, InitialModeAndNoPendingOperations) {
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }

  EXPECT_EQ(ACTIVE, [browsing_data_store_ mode]);
  EXPECT_FALSE([browsing_data_store_ hasPendingOperations]);
}

// Tests that CRWBrowsingDataStore handles several consecutive calls to
// |makeActive| and |makeInactive| correctly.
TEST_F(BrowsingDataStoreTest, MakeActiveAndInactiveOperations) {
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }

  MakeInactive();
  base::scoped_nsobject<CRWTestBrowsingDataStoreObserver> observer(
      [[CRWTestBrowsingDataStoreObserver alloc]
          initWithBrowsingDataStore:browsing_data_store_]);
  EXPECT_EQ(0U, [observer modeChangeCount]);

  __block int callbacks_received_count = 0;
  void (^unsucessfullCallback)(BOOL) = ^(BOOL success) {
    ASSERT_TRUE([NSThread isMainThread]);
    ++callbacks_received_count;
    BrowsingDataStoreMode mode = [browsing_data_store_ mode];
    EXPECT_FALSE(success);
    EXPECT_EQ(CHANGING, mode);
  };

  [browsing_data_store_ makeActiveWithCompletionHandler:^(BOOL success) {
    EXPECT_EQ(0, callbacks_received_count);
    unsucessfullCallback(success);
  }];
  EXPECT_EQ(CHANGING, [browsing_data_store_ mode]);
  EXPECT_EQ(1U, [observer modeChangeCount]);

  [browsing_data_store_ makeInactiveWithCompletionHandler:^(BOOL success) {
    EXPECT_EQ(1, callbacks_received_count);
    unsucessfullCallback(success);
  }];
  EXPECT_EQ(CHANGING, [browsing_data_store_ mode]);

  [browsing_data_store_ makeActiveWithCompletionHandler:^(BOOL success) {
    EXPECT_EQ(2, callbacks_received_count);
    unsucessfullCallback(success);
  }];
  EXPECT_EQ(CHANGING, [browsing_data_store_ mode]);

  __block BOOL block_was_called = NO;
  [browsing_data_store_ makeInactiveWithCompletionHandler:^(BOOL success) {
    ASSERT_TRUE([NSThread isMainThread]);
    EXPECT_EQ(3, callbacks_received_count);
    BrowsingDataStoreMode mode = [browsing_data_store_ mode];
    EXPECT_TRUE(success);
    EXPECT_EQ(INACTIVE, mode);
    block_was_called = YES;
  }];
  EXPECT_EQ(CHANGING, [browsing_data_store_ mode]);

  base::test::ios::WaitUntilCondition(^bool{
    return block_was_called;
  });

  EXPECT_EQ(INACTIVE, [browsing_data_store_ mode]);
  EXPECT_EQ(2U, [observer modeChangeCount]);
}

// Tests that CRWBrowsingDataStore correctly handles |removeDataOfTypes:|.
TEST_F(BrowsingDataStoreTest, RemoveDataOperations) {
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }

  ASSERT_EQ(ACTIVE, [browsing_data_store_ mode]);
  // |removeDataOfTypes| is called when the mode was ACTIVE.
  RemoveDataOfTypes(BROWSING_DATA_TYPE_COOKIES);

  MakeInactive();
  ASSERT_EQ(INACTIVE, [browsing_data_store_ mode]);
  // |removeDataOfTypes| is called when the mode was INACTIVE.
  RemoveDataOfTypes(BROWSING_DATA_TYPE_COOKIES);
}

// Tests that CRWBrowsingDataStore correctly handles |removeDataOfTypes:| after
// a |makeActive| call.
TEST_F(BrowsingDataStoreTest, RemoveDataOperationAfterMakeActiveCall) {
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }

  MakeInactive();
  ASSERT_EQ(INACTIVE, [browsing_data_store_ mode]);

  [browsing_data_store_ makeActiveWithCompletionHandler:nil];
  // |removeDataOfTypes| is called immediately after a |makeActive| call.
  RemoveDataOfTypes(BROWSING_DATA_TYPE_COOKIES);
  EXPECT_EQ(ACTIVE, [browsing_data_store_ mode]);
}

// Tests that CRWBrowsingDataStore correctly handles |removeDataOfTypes:| after
// a |makeActive| call.
TEST_F(BrowsingDataStoreTest, RemoveDataOperationAfterMakeInactiveCall) {
  if (!base::ios::IsRunningOnIOS8OrLater()) {
    return;
  }

  ASSERT_EQ(ACTIVE, [browsing_data_store_ mode]);

  [browsing_data_store_ makeInactiveWithCompletionHandler:nil];
  // |removeDataOfTypes| is called immediately after a |makeInactive| call.
  RemoveDataOfTypes(BROWSING_DATA_TYPE_COOKIES);
  EXPECT_EQ(INACTIVE, [browsing_data_store_ mode]);
}

}  // namespace web
