// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "base/logging.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/browser_coordinator.h"
#import "ios/shared/chrome/browser/coordinator_context/coordinator_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BrowserCoordinator ()
// Child coordinators owned by this object.
@property(nonatomic, strong)
    NSMutableSet<BrowserCoordinator*>* childCoordinators;
// Parent coordinator of this object, if any.
@property(nonatomic, readwrite, weak) BrowserCoordinator* parentCoordinator;
@property(nonatomic, readwrite) BOOL overlaying;
@end

@implementation BrowserCoordinator

@synthesize context = _context;
@synthesize browserState = _browserState;
@synthesize childCoordinators = _childCoordinators;
@synthesize parentCoordinator = _parentCoordinator;
@synthesize overlaying = _overlaying;

- (instancetype)init {
  if (self = [super init]) {
    _context = [[CoordinatorContext alloc] init];
    _childCoordinators = [NSMutableSet set];
  }
  return self;
}

#pragma mark - Public API

- (void)start {
  // Default implementation is a no-op.
}

- (void)stop {
  // Default implementation is a no-op.
}

@end

@implementation BrowserCoordinator (Internal)
// Concrete implementations must implement a |viewController| property.
@dynamic viewController;

- (NSSet*)children {
  return [self.childCoordinators copy];
}

- (void)addChildCoordinator:(BrowserCoordinator*)coordinator {
  CHECK([self respondsToSelector:@selector(viewController)])
      << "BrowserCoordinator implementations must provide a viewController "
         "property.";
  [self.childCoordinators addObject:coordinator];
  coordinator.parentCoordinator = self;
  coordinator.browserState = self.browserState;
  coordinator.context.baseViewController = self.viewController;
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

- (void)removeChildCoordinator:(BrowserCoordinator*)coordinator {
  if (![self.childCoordinators containsObject:coordinator])
    return;
  [self.childCoordinators removeObject:coordinator];
  coordinator.parentCoordinator = nil;
}

@end
