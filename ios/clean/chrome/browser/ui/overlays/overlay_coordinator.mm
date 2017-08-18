// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_queue.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OverlayCoordinator ()
// The queue that presented this overlay.
@property(nonatomic, assign) OverlayQueue* queue;
@end

@implementation OverlayCoordinator
@synthesize queue = _queue;

#pragma mark - Public

- (void)cancelOverlay {
  // Subclasses implement.
}

#pragma mark - BrowserCoordinator

- (void)start {
  // OverlayCoordinators should only be presented via OverlayService, and are
  // expected to be added to an OverlayQueue before being started.
  DCHECK(self.queue);
  DCHECK(self.parentCoordinator);
  [super start];
}

- (void)stop {
  DCHECK(self.queue);
  [super stop];
  [self.parentCoordinator removeChildCoordinator:self];
  self.queue->OverlayWasStopped(self);
  // Started OverlayCoordinators are owned by their OverlayQueue, and will be
  // dealocated as a result of OverlayWasStopped().  Do not add any more code
  // after this call.
}

@end

@implementation OverlayCoordinator (OverlayCoordinatorInternal)

- (void)wasAddedToQueue:(OverlayQueue*)queue {
  DCHECK(!self.queue);
  DCHECK(queue);
  self.queue = queue;
}

- (void)startOverlayingCoordinator:(BrowserCoordinator*)overlayParent {
  DCHECK(overlayParent);
  [overlayParent addChildCoordinator:self];
  [self start];
}

@end
