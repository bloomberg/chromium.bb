// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_VIEW_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_VIEW_H_

#import <UIKit/UIKit.h>

// View with the avatar on the leading side, the name and the email address of
// an identity.
// The email address is required. The name and the avatar are optional.
// If the name is empty (or nil), then the email address is vertically centered.
// The avatar is displayed as a round image.
// +--------------------------------+
// |  +------+                      |
// |  |      |  Full Name           |
// |  |Avatar|  Email address       |
// |  +------+                      |
// +--------------------------------+
@interface IdentityView : UIView

// Minimum vertical margin above and below the avatar image and title/subtitle.
// The default value is 12.
@property(nonatomic) CGFloat minimumVerticalMargin;

// Initialises IdentityView.
- (instancetype)initWithFrame:(CGRect)frame NS_DESIGNATED_INITIALIZER;

// See -[IdentityView initWithFrame:].
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Sets the avatar shown. The image is resized (40px) and shaped as a round
// image.
- (void)setAvatar:(UIImage*)avatar;

// Sets the name and the email address. |name| can be nil.
- (void)setName:(NSString*)name email:(NSString*)email;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_VIEW_H_
