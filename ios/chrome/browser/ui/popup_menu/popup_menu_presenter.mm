// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/popup_menu_presenter.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_view_controller.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PopupMenuPresenter ()
@property(nonatomic, strong) PopupMenuViewController* popupViewController;
@end

@implementation PopupMenuPresenter

@synthesize baseViewController = _baseViewController;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize guideName = _guideName;
@synthesize popupViewController = _popupViewController;
@synthesize presentedViewController = _presentedViewController;

#pragma mark - Public

- (void)prepareForPresentation {
  DCHECK(self.baseViewController);
  if (self.popupViewController)
    return;

  self.popupViewController = [[PopupMenuViewController alloc] init];
  self.popupViewController.dispatcher = self.dispatcher;
  [self.popupViewController addContent:self.presentedViewController];

  [self.baseViewController addChildViewController:self.popupViewController];
  [self.baseViewController.view addSubview:self.popupViewController.view];
  self.popupViewController.view.frame = self.baseViewController.view.frame;

  // TODO(crbug.com/804774): Prepare for animation.
  self.popupViewController.contentContainer.alpha = 0;

  [self.popupViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  // TODO(crbug.com/804774): Add animation based on |guideName|.
  self.popupViewController.contentContainer.alpha = 1;
  self.popupViewController.contentContainer.frame =
      CGRectMake(170, 300, 230, 400);
}

- (void)dismissAnimated:(BOOL)animated {
  [self.popupViewController willMoveToParentViewController:nil];
  // TODO(crbug.com/804771): Add animation.
  [self.popupViewController.view removeFromSuperview];
  [self.popupViewController removeFromParentViewController];
  self.popupViewController = nil;
  [self.delegate containedPresenterDidDismiss:self];
}

@end
