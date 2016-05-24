// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_coordinator.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state/context_menu_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

@interface ContextMenuCoordinator () {
  // Underlying system alert.
  base::scoped_nsobject<UIAlertController> _alertController;
  // View controller which will be used to present the |_alertController|.
  base::WeakNSObject<UIViewController> _presentingViewController;
  // Parameters that define what is shown in the context menu.
  web::ContextMenuParams _params;
}
// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;
// Lazy initializer to create the |_alert|.
@property(nonatomic, readonly) UIAlertController* alertController;
// Called when the alert is dismissed to perform cleanup.
- (void)alertDismissed;
@end

@implementation ContextMenuCoordinator
@synthesize visible = _visible;

- (instancetype)initWithViewController:(UIViewController*)viewController
                                params:(const web::ContextMenuParams&)params {
  self = [super init];
  if (self) {
    _params = params;
    _presentingViewController.reset(viewController);
  }
  return self;
}

#pragma mark - Object Lifecycle

- (void)dealloc {
  [self stop];
  [super dealloc];
}

#pragma mark - Public Methods

- (void)addItemWithTitle:(NSString*)title action:(ProceduralBlock)actionBlock {
  if (self.visible) {
    return;
  }
  base::WeakNSObject<ContextMenuCoordinator> weakSelf(self);
  [self.alertController
      addAction:[UIAlertAction actionWithTitle:title
                                         style:UIAlertActionStyleDefault
                                       handler:^(UIAlertAction*) {
                                         [weakSelf alertDismissed];
                                         actionBlock();
                                       }]];
}

- (void)start {
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  if (![_params.view window] &&
      ![_params.view isKindOfClass:[UIWindow class]]) {
    return;
  }

  [_presentingViewController presentViewController:self.alertController
                                          animated:YES
                                        completion:nil];
  self.visible = YES;
}

- (void)stop {
  [_alertController dismissViewControllerAnimated:NO completion:nil];
  self.visible = NO;
  _alertController.reset();
}

#pragma mark - Property Implementation

- (UIAlertController*)alertController {
  if (!_alertController) {
    DCHECK([_params.view isDescendantOfView:[_presentingViewController view]]);
    UIAlertController* alert = [UIAlertController
        alertControllerWithTitle:_params.menu_title
                         message:nil
                  preferredStyle:UIAlertControllerStyleActionSheet];
    alert.popoverPresentationController.sourceView = _params.view;
    alert.popoverPresentationController.sourceRect =
        CGRectMake(_params.location.x, _params.location.y, 1.0, 1.0);

    base::WeakNSObject<ContextMenuCoordinator> weakSelf(self);
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                                 style:UIAlertActionStyleCancel
                               handler:^(UIAlertAction*) {
                                 [weakSelf alertDismissed];
                               }];
    [alert addAction:cancelAction];

    _alertController.reset([alert retain]);
  }
  return _alertController;
}

#pragma mark - Private Methods

- (void)alertDismissed {
  self.visible = NO;
  _alertController.reset();
}

@end