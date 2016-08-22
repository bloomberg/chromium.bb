// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/update_pending_password_view_controller.h"

#import "chrome/browser/ui/cocoa/passwords/credentials_selection_view.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_list_view_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

@interface UpdatePendingPasswordViewController ()
- (void)onUpdateClicked:(id)sender;
- (void)onNopeClicked:(id)sender;
@end

@implementation UpdatePendingPasswordViewController

- (NSButton*)defaultButton {
  return updateButton_;
}

- (void)onUpdateClicked:(id)sender {
  ManagePasswordsBubbleModel* model = [self model];
  if (model) {
    if (passwordWithUsernameSelectionItem_) {
      // Multi account case.
      model->OnUpdateClicked(
          *[passwordWithUsernameSelectionItem_ getSelectedCredentials]);
    } else {
      model->OnUpdateClicked(model->pending_password());
    }
  }
  [self.delegate viewShouldDismiss];
}

- (void)onNopeClicked:(id)sender {
  ManagePasswordsBubbleModel* model = [self model];
  if (model)
    model->OnNopeUpdateClicked();
  [self.delegate viewShouldDismiss];
}

- (NSView*)createPasswordView {
  if (self.model->ShouldShowMultipleAccountUpdateUI()) {
    passwordWithUsernameSelectionItem_.reset(
        [[CredentialsSelectionView alloc] initWithModel:self.model]);
    return passwordWithUsernameSelectionItem_.get();
  } else {
    passwordItem_.reset([[PasswordsListViewController alloc]
        initWithModelAndForm:self.model
                        form:&self.model->pending_password()]);

    return [passwordItem_ view];
  }
}

- (NSArray*)createButtonsAndAddThemToView:(NSView*)view {
  // Save button.
  updateButton_.reset([[self
      addButton:l10n_util::GetNSString(IDS_PASSWORD_MANAGER_UPDATE_BUTTON)
         toView:view
         target:self
         action:@selector(onUpdateClicked:)] retain]);

  // Never button.
  NSString* nopeButtonText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_CANCEL_BUTTON);
  nopeButton_.reset([[self addButton:nopeButtonText
                              toView:view
                              target:self
                              action:@selector(onNopeClicked:)] retain]);
  return @[ updateButton_, nopeButton_ ];
}

@end

@implementation UpdatePendingPasswordViewController (Testing)

- (NSButton*)updateButton {
  return updateButton_.get();
}

- (NSButton*)noButton {
  return nopeButton_.get();
}

@end
