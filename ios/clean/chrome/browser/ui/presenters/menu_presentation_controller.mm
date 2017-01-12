// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "ios/clean/chrome/browser/ui/presenters/menu_presentation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MenuPresentationController ()
@property(nonatomic, weak) id<MenuPresentationDelegate> presentationDelegate;
@property(nonatomic, assign) CGRect presentationFrame;
@end

@implementation MenuPresentationController
@synthesize presentationDelegate = _presentationDelegate;
@synthesize presentationFrame = _presentationFrame;

#pragma mark - UIPresentationDelegate

- (CGRect)frameOfPresentedViewInContainerView {
  if (CGRectIsEmpty(self.presentationFrame)) {
    [self updatePresentationDelegate];
    if (self.presentationDelegate) {
      self.presentationFrame =
          [self.presentationDelegate frameForMenuPresentation:self];
    } else {
      // Placeholder default frame: something rectangular, 50 points in and
      // down.
      self.presentationFrame = CGRectMake(50, 50, 250, 300);
    }
  }
  return self.presentationFrame;
}

- (void)presentationTransitionWillBegin {
  self.presentedView.layer.borderWidth = 1.0;
  self.presentedView.layer.shadowRadius = 1.0;
  self.presentedView.layer.borderColor = [UIColor blackColor].CGColor;
}

#pragma mark - Private methods.

// Checks if the presenting view controller conforms to
// MenuPresentationDelegate and, if so, sets that view controller as the
// presentation delegate. This can't be done at init time, becuase the
// presenting view controller may not have been determined by UIKit yet.
- (void)updatePresentationDelegate {
  if ([self.presentingViewController
          conformsToProtocol:@protocol(MenuPresentationDelegate)]) {
    self.presentationDelegate = static_cast<id<MenuPresentationDelegate>>(
        self.presentingViewController);
  }
}

@end
