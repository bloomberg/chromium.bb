// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/common/credential_manager_types.h"

@class CredentialItemView;
class GURL;

// Handles user interaction with and image fetching for a CredentialItemView.
@protocol CredentialItemDelegate<NSObject>

// Retrieves the image located at |avatarURL| and updates |view| by calling
// [CredentialItemView updateAvatar:] if successful.
- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemView*)view;

@end

// A view to show a single account credential.
@interface CredentialItemView : NSView {
  autofill::PasswordForm passwordForm_;
  password_manager::CredentialType credentialType_;
  NSTextField* nameLabel_;
  NSTextField* usernameLabel_;
  NSImageView* avatarView_;
  id<CredentialItemDelegate> delegate_;  // Weak.
}

@property(nonatomic, readonly) autofill::PasswordForm passwordForm;
@property(nonatomic, readonly) password_manager::CredentialType credentialType;

// Initializes an item view populated with the data in |passwordForm|. Uses
// |delegate| to asynchronously fetch the avatar image.
- (id)initWithPasswordForm:(const autofill::PasswordForm&)passwordForm
            credentialType:(password_manager::CredentialType)credentialType
                  delegate:(id<CredentialItemDelegate>)delegate;

// Sets a custom avatar for this item. The image should be scaled and cropped
// to a circle of size |kAvatarImageSize|, otherwise it will look ridiculous.
- (void)updateAvatar:(NSImage*)avatar;

// The default avatar image, used when a custom one is not set.
+ (NSImage*)defaultAvatar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_VIEW_H_
