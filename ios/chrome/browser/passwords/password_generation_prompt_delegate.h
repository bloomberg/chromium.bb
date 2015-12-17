// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_DELEGATE_H_

// Handles user interaction with the PasswordGenerationPromptView.
@protocol PasswordGenerationPromptDelegate

// Indicates that the user decided to accept the generated password.
- (void)acceptPasswordGeneration:(id)sender;

// Indicates that the user wants to view their saved passwords.
- (void)showSavedPasswords:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_PROMPT_DELEGATE_H_
