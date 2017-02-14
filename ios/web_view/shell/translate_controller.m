// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/translate_controller.h"

#import <UIKit/UIKit.h>

#import "ios/web_view/public/criwv_translate_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TranslateController ()
// Action Sheet to prompt user whether or not the page should be translated.
@property(nonatomic, strong) UIAlertController* beforeTranslateActionSheet;
// Manager which performs the translation of the content.
@property(nonatomic, strong) id<CRIWVTranslateManager> translateManager;
@end

@implementation TranslateController

@synthesize beforeTranslateActionSheet = _beforeTranslateActionSheet;
@synthesize translateManager = _translateManager;

- (void)dealloc {
  [_beforeTranslateActionSheet dismissViewControllerAnimated:YES
                                                  completion:nil];
}

#pragma mark CWVTranslateDelegate methods

- (void)translateStepChanged:(CRIWVTransateStep)step
                     manager:(id<CRIWVTranslateManager>)manager {
  if (step == CRIWVTransateStepBeforeTranslate) {
    self.translateManager = manager;
    self.beforeTranslateActionSheet = [UIAlertController
        alertControllerWithTitle:nil
                         message:@"Translate?"
                  preferredStyle:UIAlertControllerStyleActionSheet];
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Nope."
                                 style:UIAlertActionStyleCancel
                               handler:^(UIAlertAction* action) {
                                 self.beforeTranslateActionSheet = nil;
                                 self.translateManager = nil;
                               }];
    [_beforeTranslateActionSheet addAction:cancelAction];

    UIAlertAction* translateAction =
        [UIAlertAction actionWithTitle:@"Yes!"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* action) {
                                 self.beforeTranslateActionSheet = nil;
                                 [_translateManager translate];
                                 self.translateManager = nil;
                               }];
    [_beforeTranslateActionSheet addAction:translateAction];

    [[UIApplication sharedApplication].keyWindow.rootViewController
        presentViewController:_beforeTranslateActionSheet
                     animated:YES
                   completion:nil];
  }
}

@end
