// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/shared/chrome/browser/ui/coordinators/browser_coordinator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TestCoordinator : BrowserCoordinator
@property(nonatomic) UIViewController* viewController;
@property(nonatomic, copy) void (^stopHandler)();
@property(nonatomic) BOOL wasAddedCalled;
@property(nonatomic) BOOL willBeRemovedCalled;
@property(nonatomic) BOOL childDidStartCalled;
@property(nonatomic) BOOL childWillStopCalled;
@end

@implementation TestCoordinator
@synthesize viewController = _viewController;
@synthesize stopHandler = _stopHandler;
@synthesize wasAddedCalled = _wasAddedCalled;
@synthesize willBeRemovedCalled = _willBeRemovedCalled;
@synthesize childDidStartCalled = _childDidStartCalled;
@synthesize childWillStopCalled = _childWillStopCalled;

- (instancetype)init {
  if (!(self = [super init]))
    return nil;

  _viewController = [[UIViewController alloc] init];
  return self;
}

- (void)stop {
  [super stop];
  if (self.stopHandler)
    self.stopHandler();
}

- (void)wasAddedToParentCoordinator:(BrowserCoordinator*)parentCoordinator {
  self.wasAddedCalled = YES;
}

- (void)willBeRemovedFromParentCoordinator {
  self.willBeRemovedCalled = YES;
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  self.childDidStartCalled = YES;
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  self.childWillStopCalled = YES;
}

@end

@interface NonOverlayableCoordinator : TestCoordinator
@end

@implementation NonOverlayableCoordinator

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  return NO;
}

@end

namespace {

TEST(BrowserCoordinatorTest, TestDontStopOnDealloc) {
  __block BOOL called = NO;

  {
    TestCoordinator* coordinator = [[TestCoordinator alloc] init];
    coordinator.stopHandler = ^{
      called = YES;
    };
  }

  EXPECT_FALSE(called);
}

TEST(BrowserCoordinatorTest, TestChildren) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];

  [parent addChildCoordinator:child];
  EXPECT_TRUE([parent.children containsObject:child]);
  EXPECT_EQ(parent, child.parentCoordinator);

  [parent removeChildCoordinator:child];
  EXPECT_FALSE([parent.children containsObject:child]);
  EXPECT_EQ(nil, child.parentCoordinator);

  TestCoordinator* otherParent = [[TestCoordinator alloc] init];
  TestCoordinator* otherChild = [[TestCoordinator alloc] init];
  [otherParent addChildCoordinator:otherChild];

  // -removeChildCoordinator of a non-child should have no affect.
  [parent removeChildCoordinator:otherChild];
  EXPECT_TRUE([otherParent.children containsObject:otherChild]);
  EXPECT_EQ(otherParent, otherChild.parentCoordinator);
}

TEST(BrowserCoordinatorTest, TestOverlay) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];
  TestCoordinator* grandchild = [[TestCoordinator alloc] init];
  TestCoordinator* overlay = [[TestCoordinator alloc] init];
  TestCoordinator* secondOverlay = [[TestCoordinator alloc] init];

  EXPECT_TRUE([parent canAddOverlayCoordinator:overlay]);
  [parent addChildCoordinator:child];
  [child addChildCoordinator:grandchild];
  EXPECT_FALSE([parent canAddOverlayCoordinator:overlay]);
  EXPECT_FALSE([child canAddOverlayCoordinator:overlay]);
  EXPECT_TRUE([grandchild canAddOverlayCoordinator:overlay]);
  EXPECT_FALSE([grandchild canAddOverlayCoordinator:child]);

  EXPECT_FALSE(overlay.overlaying);
  [parent addOverlayCoordinator:overlay];
  EXPECT_TRUE(overlay.overlaying);
  EXPECT_EQ(overlay, parent.overlayCoordinator);
  EXPECT_EQ(overlay, child.overlayCoordinator);
  EXPECT_EQ(overlay, grandchild.overlayCoordinator);
  EXPECT_TRUE([grandchild.children containsObject:overlay]);
  EXPECT_EQ(grandchild, overlay.parentCoordinator);

  // Shouldn't be able to add a second overlaying coordinator.
  EXPECT_FALSE([grandchild canAddOverlayCoordinator:secondOverlay]);
  EXPECT_FALSE(secondOverlay.overlaying);
  [parent addOverlayCoordinator:secondOverlay];
  EXPECT_FALSE(secondOverlay.overlaying);

  [child removeOverlayCoordinator];
  EXPECT_FALSE(overlay.overlaying);
  EXPECT_EQ(nil, parent.overlayCoordinator);
  EXPECT_EQ(nil, child.overlayCoordinator);
  EXPECT_EQ(nil, grandchild.overlayCoordinator);
  EXPECT_FALSE([grandchild.children containsObject:overlay]);
  EXPECT_EQ(nil, overlay.parentCoordinator);

  // An implementation that doesn't allow any overlays shouldn't get one.
  NonOverlayableCoordinator* noOverlays =
      [[NonOverlayableCoordinator alloc] init];
  TestCoordinator* thirdOverlay = [[TestCoordinator alloc] init];

  EXPECT_FALSE([noOverlays canAddOverlayCoordinator:thirdOverlay]);
  EXPECT_FALSE(thirdOverlay.overlaying);
  [noOverlays addOverlayCoordinator:thirdOverlay];
  EXPECT_FALSE(thirdOverlay.overlaying);
}

TEST(BrowserCoordinatorTest, AddedRemoved) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];

  // Add to the parent.
  EXPECT_FALSE(child.wasAddedCalled);
  EXPECT_FALSE(child.willBeRemovedCalled);
  [parent addChildCoordinator:child];
  EXPECT_TRUE(child.wasAddedCalled);
  EXPECT_FALSE(child.willBeRemovedCalled);

  // Remove from the parent.
  [parent removeChildCoordinator:child];
  EXPECT_TRUE(child.willBeRemovedCalled);
}

TEST(BrowserCoordinatorTest, DidStartWillStop) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];
  [parent addChildCoordinator:child];
  EXPECT_FALSE(parent.childDidStartCalled);
  EXPECT_FALSE(parent.childWillStopCalled);

  [child start];
  EXPECT_TRUE(parent.childDidStartCalled);
  EXPECT_FALSE(parent.childWillStopCalled);

  [child stop];
  EXPECT_TRUE(parent.childDidStartCalled);
  EXPECT_TRUE(parent.childWillStopCalled);
}

TEST(BrowserCoordinatorTest, StopStopsStartedChildren) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];
  [parent addChildCoordinator:child];
  [parent start];
  [child start];
  __block BOOL called = NO;
  child.stopHandler = ^{
    called = YES;
  };
  EXPECT_FALSE(called);

  // Call stop on the parent.
  [parent stop];

  // It should have called stop on the child.
  EXPECT_TRUE(called);
}

TEST(BrowserCoordinatorTest, StopStopsNonStartedChildren) {
  TestCoordinator* parent = [[TestCoordinator alloc] init];
  TestCoordinator* child = [[TestCoordinator alloc] init];
  [parent addChildCoordinator:child];
  [parent start];
  __block BOOL called = NO;
  child.stopHandler = ^{
    called = YES;
  };
  EXPECT_FALSE(called);

  // Call stop on the parent.
  [parent stop];

  // It should not have called stop on the child.
  EXPECT_TRUE(called);
}

}  // namespace
