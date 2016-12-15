// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/resubmit_data_controller.h"

#import "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ResubmitDataController () {
  UIAlertController* _alertController;
}
@end

@implementation ResubmitDataController

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithContinueBlock:(ProceduralBlock)continueBlock
                          cancelBlock:(ProceduralBlock)cancelBlock {
  DCHECK(continueBlock);
  DCHECK(cancelBlock);
  self = [super init];
  if (self) {
    NSString* message = [NSString
        stringWithFormat:@"%@\n\n%@",
                         l10n_util::GetNSString(IDS_HTTP_POST_WARNING_TITLE),
                         l10n_util::GetNSString(IDS_HTTP_POST_WARNING)];
    NSString* buttonTitle =
        l10n_util::GetNSString(IDS_HTTP_POST_WARNING_RESEND);
    NSString* cancelTitle = l10n_util::GetNSString(IDS_CANCEL);

    _alertController = [UIAlertController
        alertControllerWithTitle:nil
                         message:message
                  preferredStyle:UIAlertControllerStyleActionSheet];
    // Make sure the blocks are located on the heap.
    continueBlock = [continueBlock copy];
    cancelBlock = [cancelBlock copy];

    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:cancelTitle
                                 style:UIAlertActionStyleCancel
                               handler:^(UIAlertAction* _Nonnull action) {
                                 cancelBlock();
                               }];
    [_alertController addAction:cancelAction];
    UIAlertAction* continueAction =
        [UIAlertAction actionWithTitle:buttonTitle
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* _Nonnull action) {
                                 continueBlock();
                               }];
    [_alertController addAction:continueAction];
  }
  return self;
}

- (void)presentActionSheetFromRect:(CGRect)rect inView:(UIView*)view {
  _alertController.modalPresentationStyle = UIModalPresentationPopover;
  UIPopoverPresentationController* popPresenter =
      _alertController.popoverPresentationController;
  popPresenter.sourceView = view;
  popPresenter.sourceRect = rect;

  UIViewController* topController = view.window.rootViewController;
  while (topController.presentedViewController)
    topController = topController.presentedViewController;
  [topController presentViewController:_alertController
                              animated:YES
                            completion:nil];
}

- (void)dismissActionSheet {
  [_alertController.presentingViewController dismissViewControllerAnimated:YES
                                                                completion:nil];
}

@end
