// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/ios/crb_protocol_observers.h"
#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

@protocol TestObserver

@required
- (void)requiredMethod;
- (void)reset;

@optional
- (void)optionalMethod;

@end

// Implements only the required methods in the TestObserver protocol.
@interface TestPartialObserver : NSObject<TestObserver>
@property(nonatomic, readonly) BOOL requiredMethodInvoked;
@end

// Implements all the methods in the TestObserver protocol.
@interface TestCompleteObserver : TestPartialObserver<TestObserver>
@property(nonatomic, readonly) BOOL optionalMethodInvoked;
@end

namespace {

class CRBProtocolObserversTest : public PlatformTest {
 public:
  CRBProtocolObserversTest() {}

 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    observers_.reset([[CRBProtocolObservers observersWithProtocol:
        @protocol(TestObserver)] retain]);

    partial_observer_.reset([[TestPartialObserver alloc] init]);
    EXPECT_FALSE([partial_observer_ requiredMethodInvoked]);

    complete_observer_.reset([[TestCompleteObserver alloc] init]);
    EXPECT_FALSE([complete_observer_ requiredMethodInvoked]);
    EXPECT_FALSE([complete_observer_ optionalMethodInvoked]);
  }

  base::scoped_nsobject<id> observers_;
  base::scoped_nsobject<TestPartialObserver> partial_observer_;
  base::scoped_nsobject<TestCompleteObserver> complete_observer_;
};

// Verifies basic functionality of -[CRBProtocolObservers addObserver:] and
// -[CRBProtocolObservers removeObserver:].
TEST_F(CRBProtocolObserversTest, AddRemoveObserver) {
  // Add an observer and verify that the CRBProtocolObservers instance forwards
  // an invocation to it.
  [observers_ addObserver:partial_observer_];
  [observers_ requiredMethod];
  EXPECT_TRUE([partial_observer_ requiredMethodInvoked]);

  [partial_observer_ reset];
  EXPECT_FALSE([partial_observer_ requiredMethodInvoked]);

  // Remove the observer and verify that the CRBProtocolObservers instance no
  // longer forwards an invocation to it.
  [observers_ removeObserver:partial_observer_];
  [observers_ requiredMethod];
  EXPECT_FALSE([partial_observer_ requiredMethodInvoked]);
}

// Verifies that CRBProtocolObservers correctly forwards the invocation of a
// required method in the protocol.
TEST_F(CRBProtocolObserversTest, RequiredMethods) {
  [observers_ addObserver:partial_observer_];
  [observers_ addObserver:complete_observer_];
  [observers_ requiredMethod];
  EXPECT_TRUE([partial_observer_ requiredMethodInvoked]);
  EXPECT_TRUE([complete_observer_ requiredMethodInvoked]);
}

// Verifies that CRBProtocolObservers correctly forwards the invocation of an
// optional method in the protocol.
TEST_F(CRBProtocolObserversTest, OptionalMethods) {
  [observers_ addObserver:partial_observer_];
  [observers_ addObserver:complete_observer_];
  [observers_ optionalMethod];
  EXPECT_FALSE([partial_observer_ requiredMethodInvoked]);
  EXPECT_FALSE([complete_observer_ requiredMethodInvoked]);
  EXPECT_TRUE([complete_observer_ optionalMethodInvoked]);
}

// Verifies that CRBProtocolObservers only holds a weak reference to an
// observer.
TEST_F(CRBProtocolObserversTest, WeakReference) {
  base::WeakNSObject<TestPartialObserver> weak_observer(
      partial_observer_);
  EXPECT_TRUE(weak_observer);

  [observers_ addObserver:partial_observer_];

  {
    // Need an autorelease pool here, because
    // -[CRBProtocolObservers forwardInvocation:] creates a temporary
    // autoreleased array that holds all the observers.
    base::mac::ScopedNSAutoreleasePool pool;
    [observers_ requiredMethod];
    EXPECT_TRUE([partial_observer_ requiredMethodInvoked]);
  }

  partial_observer_.reset();
  EXPECT_FALSE(weak_observer.get());
}

}  // namespace

@implementation TestPartialObserver {
  BOOL _requiredMethodInvoked;
}

- (BOOL)requiredMethodInvoked {
  return _requiredMethodInvoked;
}

- (void)requiredMethod {
  _requiredMethodInvoked = YES;
}

- (void)reset {
  _requiredMethodInvoked = NO;
}

@end

@implementation TestCompleteObserver {
  BOOL _optionalMethodInvoked;
}

- (BOOL)optionalMethodInvoked {
  return _optionalMethodInvoked;
}

- (void)optionalMethod {
  _optionalMethodInvoked = YES;
}

- (void)reset {
  [super reset];
  _optionalMethodInvoked = NO;
}

@end
