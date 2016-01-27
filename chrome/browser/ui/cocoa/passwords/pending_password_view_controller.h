// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/passwords/base_passwords_content_view_controller.h"

@class PasswordsListViewController;

// Base class for the views that offer to save/update the user's password.
@interface PendingPasswordViewController
    : BasePasswordsContentViewController<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<NSButton> closeButton_;
}

// Creates a controller for showing username/password and returns its view.
- (NSView*)createPasswordView;

// Returns whether GoogleSmartLock warm welcome should be shown.
- (BOOL)shouldShowGoogleSmartLockWelcome;

// Creates buttons that should be shown in the bubble and returns them.
- (NSArray*)createButtonsAndAddThemToView:(NSView*)view;

@property(readonly) ManagePasswordsBubbleModel* model;
@end

@interface PendingPasswordViewController (Testing)
@property(readonly) NSButton* closeButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_PENDING_PASSWORD_VIEW_CONTROLLER_H_
