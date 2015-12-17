// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_UI_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/passwords/password_generation_prompt_delegate.h"

// Abstracts some UI-related tasks to isolate the rest of the code from
// Google's internal Material Design implementation.
@protocol PasswordsUiDelegate<NSObject>

// Opens the password generation alert to offer the generated |password| to the
// user.
- (void)showGenerationAlertWithPassword:(NSString*)password
                      andPromptDelegate:
                          (id<PasswordGenerationPromptDelegate>)delegate;

// Hides the alert (if open) from showGenerationAlertWithPassword.
- (void)hideGenerationAlert;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORDS_UI_DELEGATE_H_
