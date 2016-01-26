// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

class ManagePasswordsBubbleModel;
@class PasswordsListViewController;

// Manages the view that offers to save the user's password.
@interface SavePendingPasswordViewController
    : PendingPasswordViewController<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<NSButton> saveButton_;
  base::scoped_nsobject<NSButton> neverButton_;
  base::scoped_nsobject<PasswordsListViewController> passwordItem_;
}

- (NSView*)createPasswordView;
- (NSArray*)createButtonsAndAddThemToView:(NSView*)view;
@end

@interface SavePendingPasswordViewController (Testing)
@property(readonly) NSButton* saveButton;
@property(readonly) NSButton* neverButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_SAVE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
