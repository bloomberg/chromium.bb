// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_EDIT_VIEW_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_EDIT_VIEW_H_

#import <UIKit/UIKit.h>

// A view that allows the user to edit an auto-generated password.
@interface PasswordGenerationEditView : UIView
// Initializes a view with the specified |delegate| and initial value of
// |password|. Designated initializer.
- (instancetype)initWithPassword:(NSString*)password NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_GENERATION_EDIT_VIEW_H_
