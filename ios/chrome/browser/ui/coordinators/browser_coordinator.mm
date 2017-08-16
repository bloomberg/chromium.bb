// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/coordinators/browser_coordinator.h"

#import "base/logging.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserCoordinator ()
// Child coordinators owned by this object.
@property(nonatomic, strong)
    NSMutableSet<BrowserCoordinator*>* childCoordinators;
// Parent coordinator of this object, if any.
@property(nonatomic, readwrite, weak) BrowserCoordinator* parentCoordinator;
@property(nonatomic, readwrite) BOOL started;
@property(nonatomic, readwrite) BOOL overlaying;
@end

@implementation BrowserCoordinator

@synthesize browser = _browser;
@synthesize childCoordinators = _childCoordinators;
@synthesize parentCoordinator = _parentCoordinator;
@synthesize started = _started;
@synthesize overlaying = _overlaying;

- (instancetype)init {
  if (self = [super init]) {
    _childCoordinators = [NSMutableSet set];
  }
  return self;
}

#pragma mark - Public API

- (void)start {
  if (self.started) {
    return;
  }
  self.started = YES;
  [self.parentCoordinator childCoordinatorDidStart:self];
}

- (void)stop {
  if (!self.started) {
    return;
  }
  [self.parentCoordinator childCoordinatorWillStop:self];
  self.started = NO;
  for (BrowserCoordinator* child in self.children) {
    [child stop];
  }
}

- (void)dealloc {
  for (BrowserCoordinator* child in self.children) {
    [self removeChildCoordinator:child];
  }
}

@end

@implementation BrowserCoordinator (Internal)
// Concrete implementations must implement a |viewController| property.
@dynamic viewController;

- (NSSet*)children {
  return [self.childCoordinators copy];
}

- (void)addChildCoordinator:(BrowserCoordinator*)childCoordinator {
  CHECK([self respondsToSelector:@selector(viewController)])
      << "BrowserCoordinator implementations must provide a viewController "
         "property.";
  [self.childCoordinators addObject:childCoordinator];
  childCoordinator.parentCoordinator = self;
  childCoordinator.browser = self.browser;
  [childCoordinator wasAddedToParentCoordinator:self];
}

- (BrowserCoordinator*)overlayCoordinator {
  if (self.overlaying)
    return self;
  for (BrowserCoordinator* child in self.children) {
    BrowserCoordinator* overlay = child.overlayCoordinator;
    if (overlay)
      return overlay;
  }
  return nil;
}

- (void)addOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  // If this object has no children, then add |overlayCoordinator| as a child
  // and mark it as such.
  if ([self canAddOverlayCoordinator:overlayCoordinator]) {
    [self addChildCoordinator:overlayCoordinator];
    overlayCoordinator.overlaying = YES;
  } else if (self.childCoordinators.count == 1) {
    [[self.childCoordinators anyObject]
        addOverlayCoordinator:overlayCoordinator];
  } else if (self.childCoordinators.count > 1) {
    CHECK(NO) << "Coordinators with multiple children must explicitly "
              << "handle -addOverlayCoordinator: or return NO to "
              << "-canAddOverlayCoordinator:";
  }
  // If control reaches here, the terminal child of the coordinator hierarchy
  // has returned NO to -canAddOverlayCoordinator, so no overlay can be added.
  // This is by default a silent no-op.
}

- (void)removeOverlayCoordinator {
  BrowserCoordinator* overlay = self.overlayCoordinator;
  [overlay.parentCoordinator removeChildCoordinator:overlay];
  overlay.overlaying = NO;
}

- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator {
  // By default, a hierarchy with an overlay can't add a new one.
  // By default, coordinators with parents can't be added as overlays.
  // By default, coordinators with no other children can add an overlay.
  return self.overlayCoordinator == nil &&
         overlayCoordinator.parentCoordinator == nil &&
         self.childCoordinators.count == 0;
}

- (void)removeChildCoordinator:(BrowserCoordinator*)childCoordinator {
  if (![self.childCoordinators containsObject:childCoordinator])
    return;
  // Remove the grand-children first.
  for (BrowserCoordinator* grandChild in childCoordinator.children) {
    [childCoordinator removeChildCoordinator:grandChild];
  }
  // Remove the child.
  [childCoordinator willBeRemovedFromParentCoordinator];
  [self.childCoordinators removeObject:childCoordinator];
  childCoordinator.parentCoordinator = nil;
  childCoordinator.browser = nil;
}

- (void)wasAddedToParentCoordinator:(BrowserCoordinator*)parentCoordinator {
  // Default implementation is a no-op.
}

- (void)willBeRemovedFromParentCoordinator {
  // Default implementation is a no-op.
}

- (void)childCoordinatorDidStart:(BrowserCoordinator*)childCoordinator {
  // Default implementation is a no-op.
}

- (void)childCoordinatorWillStop:(BrowserCoordinator*)childCoordinator {
  // Default implementation is a no-op.
}

@end
