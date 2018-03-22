// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/shell/shell_autofill_delegate.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShellAutofillDelegate ()
// Alert controller to present autofill suggestions.
@property(nonatomic, strong) UIAlertController* alertController;

// Autofill controller.
@property(nonatomic, strong) CWVAutofillController* autofillController;

// Returns a new alert controller for picking suggestions.
- (UIAlertController*)newAlertController;

// Returns an action for a suggestion.
- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion;

@end

@implementation ShellAutofillDelegate

@synthesize alertController = _alertController;
@synthesize autofillController = _autofillController;

- (void)dealloc {
  [_alertController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - CWVAutofillControllerDelegate methods

- (void)autofillController:(CWVAutofillController*)autofillController
    didFocusOnFieldWithName:(NSString*)fieldName
             withIdentifier:(NSString*)fieldIdentifier
                   formName:(NSString*)formName
                      value:(NSString*)value {
  _autofillController = autofillController;

  __weak ShellAutofillDelegate* weakSelf = self;
  id completionHandler = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ShellAutofillDelegate* strongSelf = weakSelf;
    if (!suggestions.count || !strongSelf) {
      return;
    }

    UIAlertController* alertController = [self newAlertController];
    UIAlertAction* cancelAction =
        [UIAlertAction actionWithTitle:@"Cancel"
                                 style:UIAlertActionStyleCancel
                               handler:nil];
    [alertController addAction:cancelAction];
    for (CWVAutofillSuggestion* suggestion in suggestions) {
      [alertController addAction:[self actionForSuggestion:suggestion]];
    }
    UIAlertAction* clearAction =
        [UIAlertAction actionWithTitle:@"Clear"
                                 style:UIAlertActionStyleDefault
                               handler:^(UIAlertAction* _Nonnull action) {
                                 [autofillController clearFormWithName:formName
                                                     completionHandler:nil];
                               }];
    [alertController addAction:clearAction];

    [[UIApplication sharedApplication].keyWindow.rootViewController
        presentViewController:alertController
                     animated:YES
                   completion:nil];
    strongSelf.alertController = alertController;
  };
  [autofillController fetchSuggestionsForFormWithName:formName
                                            fieldName:fieldName
                                      fieldIdentifier:fieldIdentifier
                                    completionHandler:completionHandler];
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didInputInFieldWithName:(NSString*)fieldName
                   formName:(NSString*)formName
                      value:(NSString*)value {
  // Not implemented.
}

- (void)autofillController:(CWVAutofillController*)autofillController
    didBlurOnFieldWithName:(NSString*)fieldName
                  formName:(NSString*)formName
                     value:(NSString*)value {
  [_alertController dismissViewControllerAnimated:YES completion:nil];
  _alertController = nil;
  _autofillController = nil;
}

#pragma mark - Private Methods

- (UIAlertController*)newAlertController {
  return [UIAlertController
      alertControllerWithTitle:@"Pick a suggestion"
                       message:nil
                preferredStyle:UIAlertControllerStyleActionSheet];
}

- (UIAlertAction*)actionForSuggestion:(CWVAutofillSuggestion*)suggestion {
  NSString* title = [NSString stringWithFormat:@"%@ %@", suggestion.value,
                                               suggestion.displayDescription];
  return [UIAlertAction actionWithTitle:title
                                  style:UIAlertActionStyleDefault
                                handler:^(UIAlertAction* _Nonnull action) {
                                  [_autofillController fillSuggestion:suggestion
                                                    completionHandler:nil];
                                }];
}

@end
