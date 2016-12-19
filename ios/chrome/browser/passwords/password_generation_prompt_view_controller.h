// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class PasswordGenerationPromptDialog;

// TODO(crbug.com/636874): move this to MDC if they support alerts with subview.
// View controller used to handle the material design dialog.
@interface PasswordGenerationPromptViewController : UIViewController

// The |view| to be displayed by this ViewController.
- (instancetype)initWithPassword:(NSString*)password
                     contentView:(PasswordGenerationPromptDialog*)contentView
                  viewController:(UIViewController*)viewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_VIEW_CONTROLLER_H_
