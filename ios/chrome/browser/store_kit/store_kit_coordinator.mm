// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/store_kit/store_kit_coordinator.h"

#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface StoreKitCoordinator ()<SKStoreProductViewControllerDelegate> {
  SKStoreProductViewController* _viewController;
}
@end

@implementation StoreKitCoordinator
@synthesize iTunesItemIdentifier = _iTunesItemIdentifier;

#pragma mark - Public

- (void)start {
  DCHECK(self.iTunesItemIdentifier.length);
  DCHECK(!_viewController);
  _viewController = [[SKStoreProductViewController alloc] init];
  _viewController.delegate = self;
  NSDictionary* product = @{
    SKStoreProductParameterITunesItemIdentifier : self.iTunesItemIdentifier,
  };
  [_viewController loadProductWithParameters:product completionBlock:nil];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
}

#pragma mark - StoreKitLauncher

- (void)openAppStore:(NSString*)iTunesItemIdentifier {
  self.iTunesItemIdentifier = iTunesItemIdentifier;
  [self start];
}

#pragma mark - SKStoreProductViewControllerDelegate

- (void)productViewControllerDidFinish:
    (SKStoreProductViewController*)viewController {
  [self stop];
}

@end
