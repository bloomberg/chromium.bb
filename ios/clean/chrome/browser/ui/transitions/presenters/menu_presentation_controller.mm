// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/transitions/presenters/menu_presentation_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "ios/clean/chrome/browser/ui/commands/tools_menu_commands.h"
#include "ios/clean/chrome/browser/ui/transitions/presenters/menu_presentation_delegate.h"

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
@synthesize dispatcher = _dispatcher;
@synthesize dismissRecognizer = _dismissRecognizer;

#pragma mark - UIPresentationDelegate

- (CGRect)frameOfPresentedViewInContainerView {
  if (CGRectIsEmpty(self.presentationFrame)) {
    [self updatePresentationDelegate];
    if (self.presentationDelegate) {
      self.presentationFrame = [self
          frameForPresentationWithSize:self.presentedView.frame.size
                                origin:[self.presentationDelegate
                                               originForMenuPresentation]
                                bounds:[self.presentationDelegate
                                               boundsForMenuPresentation]];
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
  [self.dispatcher closeToolsMenu];
}

// Checks if the presenting view controller conforms to
// MenuPresentationDelegate and, if so, sets that view controller as the
// presentation delegate. This can't be done at init time, because the
// presenting view controller may not have been determined by UIKit yet.
- (void)updatePresentationDelegate {
  if ([self.presentingViewController
          conformsToProtocol:@protocol(MenuPresentationDelegate)]) {
    self.presentationDelegate = static_cast<id<MenuPresentationDelegate>>(
        self.presentingViewController);
  }
}

- (CGRect)frameForPresentationWithSize:(CGSize)menuSize
                                origin:(CGRect)menuOriginRect
                                bounds:(CGRect)presentationBounds {
  CGRect menuRect;
  menuRect.size = menuSize;

  if (CGRectIsNull(menuOriginRect)) {
    menuRect.origin = CGPointMake(50, 50);
    return menuRect;
  }
  // Calculate which corner of the menu the origin rect is in. This is
  // determined by comparing frames, and thus is RTL-independent.
  if (CGRectGetMinX(menuOriginRect) - CGRectGetMinX(presentationBounds) <
      CGRectGetMaxX(presentationBounds) - CGRectGetMaxX(menuOriginRect)) {
    // Origin rect is closer to the left edge of |self.view| than to the right.
    menuRect.origin.x = CGRectGetMinX(menuOriginRect);
  } else {
    // Origin rect is closer to the right edge of |self.view| than to the left.
    menuRect.origin.x = CGRectGetMaxX(menuOriginRect) - menuSize.width;
  }

  if (CGRectGetMinY(menuOriginRect) - CGRectGetMinY(presentationBounds) <
      CGRectGetMaxY(presentationBounds) - CGRectGetMaxY(menuOriginRect)) {
    // Origin rect is closer to the top edge of |self.view| than to the bottom.
    menuRect.origin.y = CGRectGetMinY(menuOriginRect);
  } else {
    // Origin rect is closer to the bottom edge of |self.view| than to the top.
    menuRect.origin.y = CGRectGetMaxY(menuOriginRect) - menuSize.height;
  }

  return menuRect;
}

@end
