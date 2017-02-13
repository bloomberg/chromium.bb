// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/browser/ui/presenters/menu_presentation_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "ios/clean/chrome/browser/ui/commands/toolbar_commands.h"
#include "ios/clean/chrome/browser/ui/presenters/menu_presentation_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface MenuPresentationController ()
@property(nonatomic, weak) id<MenuPresentationDelegate> presentationDelegate;
@property(nonatomic, assign) CGRect presentationFrame;
@property(nonatomic, strong) UITapGestureRecognizer* dismissRecognizer;
@end

@implementation MenuPresentationController
@synthesize presentationDelegate = _presentationDelegate;
@synthesize presentationFrame = _presentationFrame;
@synthesize toolbarCommandHandler = _toolbarCommandHandler;
@synthesize dismissRecognizer = _dismissRecognizer;

#pragma mark - UIPresentationDelegate

- (CGRect)frameOfPresentedViewInContainerView {
  if (CGRectIsEmpty(self.presentationFrame)) {
    [self updatePresentationDelegate];
    if (self.presentationDelegate) {
      self.presentationFrame =
          [self.presentationDelegate frameForMenuPresentation:self];
    } else {
      // Placeholder default frame: centered in the presenting view.
      CGSize menuSize = self.presentedView.frame.size;
      self.presentationFrame.size = menuSize;
      self.presentationFrame.origin = CGPointMake(
          (self.containerView.bounds.size.width - menuSize.width) / 2.0,
          (self.containerView.bounds.size.height - menuSize.height) / 2.0);
    }
  }
  return self.presentationFrame;
}

- (void)presentationTransitionWillBegin {
  self.presentedView.layer.shadowRadius = 5.0f;
  self.presentedView.layer.shadowOpacity = 0.4f;
  self.presentedView.layer.shadowOffset = CGSizeMake(0.0f, 0.0f);
  self.presentedView.layer.cornerRadius = 2.0f;

  self.dismissRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(tapToDismiss:)];
  [self.containerView addGestureRecognizer:self.dismissRecognizer];
}

#pragma mark - Private methods.

- (void)tapToDismiss:(UIGestureRecognizer*)recognizer {
  [self.toolbarCommandHandler closeToolsMenu];
}

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
