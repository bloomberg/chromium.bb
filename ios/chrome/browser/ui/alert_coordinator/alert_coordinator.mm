// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_block.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

@interface AlertCoordinator () {
  // Variables backing properties of the same name.
  base::scoped_nsobject<UIAlertController> _alertController;
  base::scoped_nsobject<NSString> _message;
  base::mac::ScopedBlock<ProceduralBlock> _stopAction;

  // Title for the alert.
  base::scoped_nsobject<NSString> _title;
}

// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;

// Called when the alert is dismissed to perform cleanup.
- (void)alertDismissed;

- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message;
@end

@implementation AlertCoordinator

@synthesize visible = _visible;
@synthesize cancelButtonAdded = _cancelButtonAdded;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                     title:(NSString*)title
                                   message:(NSString*)message {
  self = [super initWithBaseViewController:viewController];
  if (self) {
    _title.reset([title copy]);
    _message.reset([message copy]);
  }
  return self;
}

#pragma mark - Public Methods.

- (void)addItemWithTitle:(NSString*)title
                  action:(ProceduralBlock)actionBlock
                   style:(UIAlertActionStyle)style {
  if (self.visible ||
      (style == UIAlertActionStyleCancel && self.cancelButtonAdded)) {
    return;
  }

  if (style == UIAlertActionStyleCancel)
    _cancelButtonAdded = YES;

  base::WeakNSObject<AlertCoordinator> weakSelf(self);

  UIAlertAction* alertAction =
      [UIAlertAction actionWithTitle:title
                               style:style
                             handler:^(UIAlertAction*) {
                               if (actionBlock)
                                 actionBlock();
                               [weakSelf alertDismissed];
                             }];

  [self.alertController addAction:alertAction];
}

- (void)start {
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  if (![self.baseViewController.view window] &&
      ![self.baseViewController.view isKindOfClass:[UIWindow class]]) {
    return;
  }

  // Display at least one button to let the user dismiss the alert.
  if ([self.alertController actions].count == 0) {
    [self addItemWithTitle:l10n_util::GetNSString(IDS_APP_OK)
                    action:nil
                     style:UIAlertActionStyleDefault];
  }

  [self.baseViewController presentViewController:self.alertController
                                        animated:YES
                                      completion:nil];
  self.visible = YES;
}

- (void)stop {
  [_alertController dismissViewControllerAnimated:NO completion:nil];
  if (_stopAction)
    _stopAction.get()();
  [self alertDismissed];
}

#pragma mark - Property Implementation.

- (UIAlertController*)alertController {
  if (!_alertController) {
    UIAlertController* alert =
        [self alertControllerWithTitle:_title message:_message];

    if (alert)
      _alertController.reset([alert retain]);
  }
  return _alertController;
}

- (NSString*)message {
  return _message;
}

- (void)setMessage:(NSString*)message {
  _message.reset([message copy]);
}

- (ProceduralBlock)stopAction {
  return _stopAction;
}

- (void)setStopAction:(ProceduralBlock)stopAction {
  _stopAction.reset([stopAction copy]);
}

#pragma mark - Private Methods.

- (void)alertDismissed {
  self.visible = NO;
  _cancelButtonAdded = NO;
  _alertController.reset();
  _stopAction.reset();
}

- (UIAlertController*)alertControllerWithTitle:(NSString*)title
                                       message:(NSString*)message {
  return
      [UIAlertController alertControllerWithTitle:title
                                          message:message
                                   preferredStyle:UIAlertControllerStyleAlert];
}

@end