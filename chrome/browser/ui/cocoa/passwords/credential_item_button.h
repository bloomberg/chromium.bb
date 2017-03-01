// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_BUTTON_H_

#import <Cocoa/Cocoa.h>

#include "components/password_manager/core/common/credential_manager_types.h"
#import "ui/base/cocoa/hover_button.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

// A custom button that allows for setting a background color when hovered over.
@interface CredentialItemButton : HoverButton
@property(nonatomic) autofill::PasswordForm* passwordForm;
@property(nonatomic) password_manager::CredentialType credentialType;

- (id)initWithFrame:(NSRect)frameRect
    backgroundColor:(NSColor*)backgroundColor
         hoverColor:(NSColor*)hoverColor;

// Adds (i) icon with a tooltip to the view.
- (NSView*)addInfoIcon:(NSString*)tooltip;

// Returns an image to be used as a default avatar.
+ (NSImage*)defaultAvatar;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_CREDENTIAL_ITEM_BUTTON_H_
