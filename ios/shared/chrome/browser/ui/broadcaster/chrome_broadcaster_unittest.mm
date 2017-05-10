// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

#import "ios/shared/chrome/browser/ui/broadcaster/chrome_broadcast_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/perf/perf_test.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestObserver : NSObject<ChromeBroadcastObserver>
@property(nonatomic) BOOL lastObservedBool;
@property(nonatomic) CGFloat lastObservedCGFloat;
@property(nonatomic) NSInteger tabStripVisibleCallCount;
@property(nonatomic) NSInteger contentScrollOffsetCallCount;
@end

@implementation TestObserver
@synthesize lastObservedBool = _lastObservedBool;
@synthesize lastObservedCGFloat = _lastObservedCGFloat;
@synthesize tabStripVisibleCallCount = _tabStripVisibleCallCount;
@synthesize contentScrollOffsetCallCount = _contentScrollOffsetCallCount;

- (void)broadcastTabStripVisible:(BOOL)visible {
  self.tabStripVisibleCallCount++;
  self.lastObservedBool = visible;
}

- (void)broadcastContentScrollOffset:(CGFloat)offset {
  self.contentScrollOffsetCallCount++;
  self.lastObservedCGFloat = offset;
}

@end

@interface TestObservable : NSObject
@property(nonatomic) BOOL observableBool;
@property(nonatomic) CGFloat observableCGFloat;
@end
@implementation TestObservable
@synthesize observableBool = _observableBool;
@synthesize observableCGFloat = _observableCGFloat;
@end

typedef PlatformTest ChromeBroadcasterTest;

TEST_F(ChromeBroadcasterTest, TestBroadcastBoolFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableBool = NO;

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastTabStripVisible:)];

  observable.observableBool = YES;

  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastTabStripVisible:)];
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  EXPECT_TRUE(observer.lastObservedBool);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestBroadcastFloatFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveBoolFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastTabStripVisible:)];
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableBool = YES;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(0, observer.tabStripVisibleCallCount);

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastTabStripVisible:)];
  EXPECT_TRUE(observer.lastObservedBool);
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestObserveFloatFirst) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* observer = [[TestObserver alloc] init];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;
  EXPECT_EQ(0.0, observer.lastObservedCGFloat);
  EXPECT_EQ(0, observer.contentScrollOffsetCallCount);

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(1.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);

  observable.observableCGFloat = 2.0;
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestBroadcastManyFloats) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  NSMutableArray<TestObserver*>* observers = [[NSMutableArray alloc] init];
  for (size_t i = 0; i < 100; i++) {
    [observers addObject:[[TestObserver alloc] init]];
    [broadcaster addObserver:observers.lastObject
                 forSelector:@selector(broadcastContentScrollOffset:)];
  }

  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];
  // All observers should have the initial value set.
  for (TestObserver* observer in observers) {
    EXPECT_EQ(1.0, observer.lastObservedCGFloat);
    EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  }

  // Change the value a thousand times.
  NSDate* start = [NSDate date];
  for (size_t i = 0; i < 1000; i++) {
    observable.observableCGFloat += 1.0;
  }
  NSTimeInterval elapsed = -[start timeIntervalSinceNow] * 1000.0 /* to ms */;

  // Log the elapsed time for performance tracking.
  perf_test::PrintResult("Broadcast", "", "100 observers, 1000 updates",
                         elapsed, "ms", true /* "important" */);

  EXPECT_EQ(1001.0, observable.observableCGFloat);
  for (TestObserver* observer in observers) {
    EXPECT_EQ(1001.0, observer.lastObservedCGFloat);
    EXPECT_EQ(1001, observer.contentScrollOffsetCallCount);
  }
}

TEST_F(ChromeBroadcasterTest, TestBroadcastDuplicateFloats) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 2.0;
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestSeparateObservers) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObserver* boolObserver = [[TestObserver alloc] init];
  TestObserver* floatObserver = [[TestObserver alloc] init];

  TestObservable* observable = [[TestObservable alloc] init];

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastTabStripVisible:)];
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  [broadcaster addObserver:boolObserver
               forSelector:@selector(broadcastTabStripVisible:)];
  [broadcaster addObserver:floatObserver
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_FALSE(boolObserver.lastObservedBool);
  EXPECT_EQ(1, boolObserver.tabStripVisibleCallCount);
  EXPECT_EQ(0, floatObserver.tabStripVisibleCallCount);
  EXPECT_EQ(0.0, floatObserver.lastObservedCGFloat);
  EXPECT_EQ(1, floatObserver.contentScrollOffsetCallCount);
  EXPECT_EQ(0, boolObserver.contentScrollOffsetCallCount);

  observable.observableCGFloat = 5.0;
  EXPECT_EQ(5.0, floatObserver.lastObservedCGFloat);
  EXPECT_EQ(2, floatObserver.contentScrollOffsetCallCount);
  EXPECT_EQ(0.0, boolObserver.lastObservedCGFloat);
  EXPECT_EQ(0, boolObserver.contentScrollOffsetCallCount);

  observable.observableBool = YES;
  EXPECT_TRUE(boolObserver.lastObservedBool);
  EXPECT_EQ(2, boolObserver.tabStripVisibleCallCount);
  EXPECT_FALSE(floatObserver.lastObservedBool);
  EXPECT_EQ(0, floatObserver.tabStripVisibleCallCount);
}

TEST_F(ChromeBroadcasterTest, TestStopBroadcasting) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;

  TestObserver* observer = [[TestObserver alloc] init];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  [broadcaster
      stopBroadcastingForSelector:@selector(broadcastContentScrollOffset:)];
  observable.observableCGFloat = 4.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
}

TEST_F(ChromeBroadcasterTest, TestStopObserving) {
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  TestObservable* observable = [[TestObservable alloc] init];
  observable.observableCGFloat = 1.0;

  [broadcaster broadcastValue:@"observableBool"
                     ofObject:observable
                     selector:@selector(broadcastTabStripVisible:)];
  [broadcaster broadcastValue:@"observableCGFloat"
                     ofObject:observable
                     selector:@selector(broadcastContentScrollOffset:)];

  observable.observableCGFloat = 2.0;
  observable.observableBool = YES;
  TestObserver* observer = [[TestObserver alloc] init];

  [broadcaster addObserver:observer
               forSelector:@selector(broadcastTabStripVisible:)];
  [broadcaster addObserver:observer
               forSelector:@selector(broadcastContentScrollOffset:)];
  EXPECT_EQ(2.0, observer.lastObservedCGFloat);
  EXPECT_EQ(1, observer.contentScrollOffsetCallCount);
  EXPECT_TRUE(observer.lastObservedBool);
  EXPECT_EQ(1, observer.tabStripVisibleCallCount);
  observable.observableCGFloat = 3.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  [broadcaster removeObserver:observer
                  forSelector:@selector(broadcastContentScrollOffset:)];
  observable.observableCGFloat = 4.0;
  EXPECT_EQ(3.0, observer.lastObservedCGFloat);
  EXPECT_EQ(2, observer.contentScrollOffsetCallCount);
  observable.observableBool = NO;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
  [broadcaster removeObserver:observer
                  forSelector:@selector(broadcastTabStripVisible:)];
  observable.observableBool = YES;
  EXPECT_FALSE(observer.lastObservedBool);
  EXPECT_EQ(2, observer.tabStripVisibleCallCount);
}
