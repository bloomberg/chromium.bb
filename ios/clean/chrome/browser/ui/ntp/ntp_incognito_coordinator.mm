// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/ntp/ntp_incognito_coordinator.h"

#import "ios/chrome/browser/ui/coordinators/browser_coordinator+internal.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller_delegate.h"
#import "ios/clean/chrome/browser/ui/adaptor/url_loader_adaptor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NTPIncognitoCoordinator ()<IncognitoViewControllerDelegate>

@property(nonatomic, strong) UIViewController* viewController;
@property(nonatomic, strong) URLLoaderAdaptor* loader;

@end

@implementation NTPIncognitoCoordinator

@synthesize viewController = _viewController;
@synthesize loader = _loader;

- (void)start {
  self.loader = [[URLLoaderAdaptor alloc] init];
  self.viewController =
      [[IncognitoViewController alloc] initWithLoader:self.loader
                                      toolbarDelegate:self];
  [super start];
}

#pragma mark - IncognitoViewControllerDelegate

// TODO(crbug.com/740793): This should be handled by the broadcaster. Removes
// this method once it is done.
- (void)setToolbarBackgroundAlpha:(CGFloat)alpha {
  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:@"setToolbarBackgroundAlpha:"
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* action =
      [UIAlertAction actionWithTitle:@"Done"
                               style:UIAlertActionStyleCancel
                             handler:nil];
  [alertController addAction:action];
  [self.viewController presentViewController:alertController
                                    animated:YES
                                  completion:nil];
}

@end
