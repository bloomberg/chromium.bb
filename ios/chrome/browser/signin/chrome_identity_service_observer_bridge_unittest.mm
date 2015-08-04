// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/chrome_identity_service_observer_bridge.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "testing/platform_test.h"

@interface TestChromeIdentityServiceObserver
    : NSObject<ChromeIdentityServiceObserver>
@property(nonatomic) BOOL onIdentityListChangedCalled;
@property(nonatomic) BOOL onAccessTokenRefreshFailedCalled;
@property(nonatomic) BOOL onProfileUpdateCalled;
@property(nonatomic, assign) ChromeIdentity* identity;
@property(nonatomic) ios::AccessTokenErrorReason error;
@property(nonatomic, readonly)
    ios::ChromeIdentityService::Observer* observerBridge;
@end

@implementation TestChromeIdentityServiceObserver {
  scoped_ptr<ios::ChromeIdentityService::Observer> observer_bridge_;
}

@synthesize onIdentityListChangedCalled = _onIdentityListChangedCalled;
@synthesize onAccessTokenRefreshFailedCalled =
    _onAccessTokenRefreshFailedCalled;
@synthesize onProfileUpdateCalled = _onProfileUpdateCalled;
@synthesize identity = _identity;
@synthesize error = _error;

- (instancetype)init {
  if (self == [super init]) {
    observer_bridge_.reset(new ChromeIdentityServiceObserverBridge(self));
  }
  return self;
}

- (ios::ChromeIdentityService::Observer*)observerBridge {
  return observer_bridge_.get();
}

#pragma mark - ios::ChromeIdentityService::Observer

- (void)onIdentityListChanged {
  _onIdentityListChangedCalled = YES;
}

- (void)onAccessTokenRefreshFailed:(ChromeIdentity*)identity
                             error:(ios::AccessTokenErrorReason)error {
  _onAccessTokenRefreshFailedCalled = YES;
  _error = error;
  _identity = identity;
}

- (void)onProfileUpdate:(ChromeIdentity*)identity {
  _onProfileUpdateCalled = YES;
  _identity = identity;
}

@end

#pragma mark - ChromeIdentityServiceObserverBridgeTest

class ChromeIdentityServiceObserverBridgeTest : public PlatformTest {
 protected:
  ChromeIdentityServiceObserverBridgeTest()
      : test_observer_([[TestChromeIdentityServiceObserver alloc] init]) {}

  ios::ChromeIdentityService::Observer* GetObserverBridge() {
    return [test_observer_ observerBridge];
  }

  TestChromeIdentityServiceObserver* GetTestObserver() {
    return test_observer_;
  }

 private:
  base::scoped_nsobject<TestChromeIdentityServiceObserver> test_observer_;
};

// Tests that |onIdentityListChanged| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onIdentityListChanged) {
  ASSERT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  GetObserverBridge()->OnIdentityListChanged();
  EXPECT_TRUE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_FALSE(GetTestObserver().onProfileUpdateCalled);
}

// Tests that |onAccessTokenRefreshFailed| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onAccessTokenRefreshFailed) {
  base::scoped_nsobject<ChromeIdentity> identity([[ChromeIdentity alloc] init]);
  ios::AccessTokenErrorReason error =
      ios::AccessTokenErrorReason::UNKNOWN_ERROR;
  ASSERT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  GetObserverBridge()->OnAccessTokenRefreshFailed(identity, error);
  EXPECT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_TRUE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_FALSE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_EQ(identity, GetTestObserver().identity);
  EXPECT_EQ(error, GetTestObserver().error);
}

// Tests that |onProfileUpdate| is forwarded.
TEST_F(ChromeIdentityServiceObserverBridgeTest, onProfileUpdate) {
  base::scoped_nsobject<ChromeIdentity> identity([[ChromeIdentity alloc] init]);
  ASSERT_FALSE(GetTestObserver().onProfileUpdateCalled);
  GetObserverBridge()->OnProfileUpdate(identity);
  EXPECT_FALSE(GetTestObserver().onIdentityListChangedCalled);
  EXPECT_FALSE(GetTestObserver().onAccessTokenRefreshFailedCalled);
  EXPECT_TRUE(GetTestObserver().onProfileUpdateCalled);
  EXPECT_EQ(identity, GetTestObserver().identity);
}
