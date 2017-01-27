// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/translate_controller.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web_view/public/criwv_translate_manager.h"

@interface TranslateController () {
  base::scoped_nsobject<UIAlertController> _beforeTranslateActionSheet;
  base::scoped_nsprotocol<id<CRIWVTranslateManager>> _translateManager;
}
@end

@implementation TranslateController

- (void)dealloc {
  [_beforeTranslateActionSheet dismissViewControllerAnimated:YES
                                                  completion:nil];
  [super dealloc];
}

#pragma mark CRIWVTranslateDelegate methods

- (void)translateStepChanged:(CRIWVTransateStep)step
                     manager:(id<CRIWVTranslateManager>)manager {
  if (step == CRIWVTransateStepBeforeTranslate) {
    DCHECK(!_translateManager);
    DCHECK(!_beforeTranslateActionSheet);
    _translateManager.reset([manager retain]);
    _beforeTranslateActionSheet.reset([[UIAlertController
        alertControllerWithTitle:nil
                         message:@"Translate?"
                  preferredStyle:UIAlertControllerStyleActionSheet] retain]);
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Nope."
                                 style:UIAlertActionStyleCancel
                               handler:^(UIAlertAction* action) {
                                 DCHECK(_beforeTranslateActionSheet);
                                 _beforeTranslateActionSheet.reset();
                                 DCHECK(_translateManager);
                                 _translateManager.reset();
                               }];
    [_beforeTranslateActionSheet addAction:cancelAction];

    UIAlertAction* translateAction =
        [UIAlertAction actionWithTitle:@"Yes!"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 DCHECK(_beforeTranslateActionSheet);
                                 _beforeTranslateActionSheet.reset();
                                 DCHECK(_translateManager);
                                 [_translateManager translate];
                                 _translateManager.reset();
                               }];
    [_beforeTranslateActionSheet addAction:translateAction];

    [[UIApplication sharedApplication].keyWindow.rootViewController
        presentViewController:_beforeTranslateActionSheet
                     animated:YES
                   completion:nil];
  }
}

@end
