// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/resubmit_data_controller.h"

#import "base/logging.h"
#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

@interface ResubmitDataController () {
  base::scoped_nsobject<UIAlertController> _alertController;
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

    _alertController.reset([[UIAlertController
        alertControllerWithTitle:nil
                         message:message
                  preferredStyle:UIAlertControllerStyleActionSheet] retain]);

    // Make sure the blocks are located on the heap.
    base::mac::ScopedBlock<ProceduralBlock> guaranteeOnHeap;
    guaranteeOnHeap.reset([continueBlock copy]);
    guaranteeOnHeap.reset([cancelBlock copy]);

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
  _alertController.get().modalPresentationStyle = UIModalPresentationPopover;
  UIPopoverPresentationController* popPresenter =
      _alertController.get().popoverPresentationController;
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
  [_alertController.get().presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

@end
