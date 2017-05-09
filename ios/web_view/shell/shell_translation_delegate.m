// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_translation_delegate.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellTranslationDelegate ()
// Action Sheet to prompt user whether or not the page should be translated.
@property(nonatomic, strong) UIAlertController* beforeTranslateActionSheet;
@end

@implementation ShellTranslationDelegate

@synthesize beforeTranslateActionSheet = _beforeTranslateActionSheet;

- (void)dealloc {
  [_beforeTranslateActionSheet dismissViewControllerAnimated:YES
                                                  completion:nil];
}

#pragma mark - CWVTranslationDelegate methods

- (void)translationController:(CWVTranslationController*)controller
    didFinishLanguageDetectionWithResult:(CWVLanguageDetectionResult*)result
                                   error:(NSError*)error {
  __weak ShellTranslationDelegate* weakSelf = self;

  self.beforeTranslateActionSheet = [UIAlertController
      alertControllerWithTitle:nil
                       message:@"Translate?"
                preferredStyle:UIAlertControllerStyleActionSheet];
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:@"Nope."
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               weakSelf.beforeTranslateActionSheet = nil;
                             }];
  [_beforeTranslateActionSheet addAction:cancelAction];

  UIAlertAction* translateAction = [UIAlertAction
      actionWithTitle:@"Yes!"
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                weakSelf.beforeTranslateActionSheet = nil;
                CWVTranslationLanguage* source = result.pageLanguage;
                CWVTranslationLanguage* target = result.suggestedTargetLanguage;
                [controller translatePageFromLanguage:source toLanguage:target];
              }];
  [_beforeTranslateActionSheet addAction:translateAction];

  [[UIApplication sharedApplication].keyWindow.rootViewController
      presentViewController:_beforeTranslateActionSheet
                   animated:YES
                 completion:nil];
}

@end
