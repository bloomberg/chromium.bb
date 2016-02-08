// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTOSIGNIN_PROMPT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTOSIGNIN_PROMPT_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/passwords/password_prompt_bridge_interface.h"

// Manages a view that shows an auto signin dialog content shown as a first run
// experience.
@interface AutoSigninPromptViewController
    : NSViewController<PasswordPromptViewInterface,
                       NSTextViewDelegate>
@property(nonatomic) PasswordPromptBridgeInterface* bridge;
- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge;
@end

@interface AutoSigninPromptViewController(Testing)
@property(nonatomic, readonly) NSTextView* contentText;
@property(nonatomic, readonly) NSButton* okButton;
@property(nonatomic, readonly) NSButton* turnOffButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_AUTOSIGNIN_PROMPT_VIEW_CONTROLLER_H_
