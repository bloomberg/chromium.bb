// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/context_menu_controller.h"

#include <algorithm>

#include "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/context_menu/context_menu_holder.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

@interface ContextMenuController () {
  // Underlying system alert.
  base::WeakNSObject<UIAlertController> _alert;
}
// Redefined to readwrite.
@property(nonatomic, readwrite, getter=isVisible) BOOL visible;
@end

@implementation ContextMenuController
@synthesize visible = _visible;

- (void)dealloc {
  [_alert dismissViewControllerAnimated:NO completion:nil];
  [super dealloc];
}

- (void)showWithHolder:(ContextMenuHolder*)menuHolder
               atPoint:(CGPoint)point
                inView:(UIView*)view {
  DCHECK(menuHolder.itemCount);
  // Check that the view is still visible on screen, otherwise just return and
  // don't show the context menu.
  if (![view window] && ![view isKindOfClass:[UIWindow class]])
    return;

  UIAlertController* alert = [UIAlertController
      alertControllerWithTitle:menuHolder.menuTitle
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
  alert.popoverPresentationController.sourceView = view;
  alert.popoverPresentationController.sourceRect =
      CGRectMake(point.x, point.y, 1.0, 1.0);

  // Add the actions.
  base::WeakNSObject<ContextMenuController> weakSelf(self);
  [menuHolder.itemTitles enumerateObjectsUsingBlock:^(
                             NSString* itemTitle, NSUInteger itemIndex, BOOL*) {
    void (^actionHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
      [menuHolder performActionAtIndex:itemIndex];
      [weakSelf setVisible:NO];
    };
    [alert addAction:[UIAlertAction actionWithTitle:itemTitle
                                              style:UIAlertActionStyleDefault
                                            handler:actionHandler]];
  }];

  // Cancel button goes last, to match other browsers.
  void (^cancelHandler)(UIAlertAction*) = ^(UIAlertAction* action) {
    [weakSelf setVisible:NO];
  };
  UIAlertAction* cancel_action =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                               style:UIAlertActionStyleCancel
                             handler:cancelHandler];
  [alert addAction:cancel_action];

  // Present sheet/popover using controller that is added to view hierarchy.
  UIViewController* topController = view.window.rootViewController;
  while (topController.presentedViewController)
    topController = topController.presentedViewController;
  [topController presentViewController:alert animated:YES completion:nil];
  self.visible = YES;
  _alert.reset(alert);
}

- (void)dismissAnimated:(BOOL)animated
      completionHandler:(ProceduralBlock)completionHandler {
  [_alert dismissViewControllerAnimated:animated completion:completionHandler];
}

@end
