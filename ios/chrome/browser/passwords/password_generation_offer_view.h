// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_OFFER_VIEW_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_OFFER_VIEW_H_

#import <UIKit/UIKit.h>

// Handles user interaction with the PasswordGenerationOfferView.
@protocol PasswordGenerationOfferDelegate<NSObject>
// Indicates that the user wants an auto-generated password.
- (void)generatePassword;
@end

// A view that offers to generate a password automatically.
@interface PasswordGenerationOfferView : UIView
// Initializes a view with the specified |delegate|.
- (instancetype)initWithDelegate:(id<PasswordGenerationOfferDelegate>)delegate;
@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_OFFER_VIEW_H_
