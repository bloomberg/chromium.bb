// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/passwords/password_prompt_bridge_interface.h"

@class AccountAvatarFetcherManager;
class PasswordPromptBridgeInterface;

// Manages a view that shows a list of credentials that can be used for
// authentication to a site.
@interface AccountChooserViewController
    : NSViewController<PasswordPromptViewInterface, NSTextViewDelegate>
@property(nonatomic) PasswordPromptBridgeInterface* bridge;
// Initializes a new account chooser and populates it with the credentials
// stored in |bridge->controller()|.
- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge;
@end

@interface AccountChooserViewController(Testing)
- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge
                 avatarManager:(AccountAvatarFetcherManager*)avatarManager;
@property(nonatomic, readonly) NSButton* cancelButton;
@property(nonatomic, readonly) NSArray* credentialButtons;
@property(nonatomic, readonly) NSTextView* titleView;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_CHOOSER_VIEW_CONTROLLER_H_
