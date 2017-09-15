// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/dialogs/unavailable_feature_dialogs/unavailable_feature_dialog_coordinator.h"

#import "ios/chrome/browser/ui/browser_list/browser.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/clean/chrome/browser/ui/commands/unavailable_feature_dialog_commands.h"
#import "ios/clean/chrome/browser/ui/dialogs/dialog_coordinator+subclassing.h"
#import "ios/clean/chrome/browser/ui/dialogs/unavailable_feature_dialogs/unavailable_feature_dialog_mediator.h"
#import "ios/clean/chrome/browser/ui/overlays/overlay_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UnavailableFeatureDialogCoordinator ()<
    UnavailableFeatureDialogDismissalCommands> {
  // Backing object for property of same name (from Subclassing category).
  UnavailableFeatureDialogMediator* _mediator;
}

// The dispatcher to use for UnavailableFeatureMediators.
@property(nonatomic, readonly) id<UnavailableFeatureDialogDismissalCommands>
    callableDispatcher;
// The name of the unavailable feature.
@property(nonatomic, copy, readonly) NSString* featureName;

@end

@implementation UnavailableFeatureDialogCoordinator
@dynamic callableDispatcher;
@synthesize featureName = _featureName;

- (instancetype)initWithFeatureName:(NSString*)featureName {
  DCHECK(featureName.length);
  if (self = [super init]) {
    _featureName = [featureName copy];
  }
  return self;
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;
  _mediator = [[UnavailableFeatureDialogMediator alloc]
      initWithFeatureName:self.featureName];
  _mediator.dispatcher = self.callableDispatcher;
  [self.dispatcher
      startDispatchingToTarget:self
                   forProtocol:@protocol(
                                   UnavailableFeatureDialogDismissalCommands)];
  [super start];
}

- (void)stop {
  if (!self.started)
    return;
  [self.dispatcher stopDispatchingToTarget:self];
  [super stop];
}

#pragma mark - UnavailableFeatureDismisalCommands

- (void)dismissUnavailableFeatureDialog {
  [self stop];
}

@end

@implementation UnavailableFeatureDialogCoordinator (
    DialogCoordinatorSubclassing)

- (DialogMediator*)mediator {
  return _mediator;
}

@end
