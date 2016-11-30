// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/action_sheet_coordinator.h"

#import "base/mac/scoped_nsobject.h"

@interface ActionSheetCoordinator () {
  // Rectangle for the popover alert.
  CGRect _rect;
  // View for the popovert alert.
  base::scoped_nsobject<UIView> _view;
}

@end

@implementation ActionSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message
                                      rect:(CGRect)rect
                                      view:(UIView*)view {
  self = [super initWithBaseViewController:viewController
                                     title:title
                                   message:message];
  if (self) {
    _rect = rect;
    _view.reset([view retain]);
  }
  return self;
}

- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message {
  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:title
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];

  alert.popoverPresentationController.sourceView = _view;
  alert.popoverPresentationController.sourceRect = _rect;

  return alert;
}

@end
