// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_UPDATE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_UPDATE_PENDING_PASSWORD_VIEW_CONTROLLER_H_

#import "chrome/browser/ui/cocoa/passwords/pending_password_view_controller.h"

class ManagePasswordsBubbleModel;
@class CredentialsSelectionView;
@class PasswordsListViewController;

// Manages the view that offers to save the user's password.
@interface UpdatePendingPasswordViewController
    : PendingPasswordViewController<NSTextViewDelegate> {
 @private
  base::scoped_nsobject<NSButton> updateButton_;
  base::scoped_nsobject<NSButton> nopeButton_;
  base::scoped_nsobject<PasswordsListViewController> passwordItem_;
  base::scoped_nsobject<CredentialsSelectionView>
      passwordWithUsernameSelectionItem_;
}

- (NSView*)createPasswordView;
- (NSArray*)createButtonsAndAddThemToView:(NSView*)view;
@end

@interface UpdatePendingPasswordViewController (Testing)
@property(readonly) NSButton* updateButton;
@property(readonly) NSButton* noButton;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_UPDATE_PENDING_PASSWORD_VIEW_CONTROLLER_H_
