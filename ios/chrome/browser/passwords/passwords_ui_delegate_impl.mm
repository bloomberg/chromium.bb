// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/passwords_ui_delegate_impl.h"

#import "ios/chrome/browser/passwords/password_generation_prompt_view.h"
#import "ios/chrome/browser/passwords/password_generation_prompt_view_controller.h"

@implementation PasswordsUiDelegateImpl

#pragma mark -
#pragma mark PasswordsUiDelegate

- (void)showGenerationAlertWithPassword:(NSString*)password
                      andPromptDelegate:
                          (id<PasswordGenerationPromptDelegate>)delegate {
  UIViewController* topViewController =
      [[[UIApplication sharedApplication] keyWindow] rootViewController];

  // Look for the top-most presented ViewController.
  for (UIViewController* controller = topViewController.presentedViewController;
       controller && ![controller isBeingDismissed];
       controller = controller.presentedViewController) {
    // Return if a PasswordGenerationPromptViewController is already presented.
    if ([controller
            isKindOfClass:[PasswordGenerationPromptViewController class]])
      return;

    topViewController = controller;
  }

  PasswordGenerationPromptDialog* contentView =
      [[[PasswordGenerationPromptDialog alloc]
          initWithDelegate:delegate
            viewController:topViewController] autorelease];

  UIViewController* viewController =
      [[[PasswordGenerationPromptViewController alloc]
          initWithPassword:password
               contentView:contentView
            viewController:topViewController] autorelease];

  [topViewController presentViewController:viewController
                                  animated:YES
                                completion:nil];
}

- (void)hideGenerationAlert {
  UIViewController* rootViewController =
      [[[UIApplication sharedApplication] keyWindow] rootViewController];

  // Check every presented ViewController for a password generation prompt.
  for (UIViewController* controller = rootViewController;
       controller && ![controller isBeingDismissed];
       controller = controller.presentedViewController) {
    if ([controller.presentedViewController
            isKindOfClass:[PasswordGenerationPromptViewController class]]) {
      // Dismiss the password prompt.
      [controller dismissViewControllerAnimated:NO completion:nil];
      return;
    }
  }
}

@end
