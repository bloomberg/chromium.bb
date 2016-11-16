// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/credentials_selection_view.h"

#include <stddef.h>

#import "chrome/browser/ui/cocoa/bubble_combobox.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "ui/base/models/simple_combobox_model.h"

namespace {

NSPopUpButton* CreateUsernamesPopUpButton(
    const std::vector<autofill::PasswordForm>& forms,
    const base::string16& best_matched_username) {
  DCHECK(!forms.empty());
  std::vector<base::string16> usernames;
  size_t best_matched_username_index = forms.size();
  size_t preffered_form_index = forms.size();

  for (size_t index = 0; index < forms.size(); ++index) {
    usernames.push_back(forms[index].username_value);
    if (forms[index].username_value == best_matched_username) {
      best_matched_username_index = index;
    }
    if (forms[index].preferred) {
      preffered_form_index = index;
    }
  }

  ui::SimpleComboboxModel model(usernames);
  base::scoped_nsobject<NSPopUpButton> button([[BubbleCombobox alloc]
      initWithFrame:NSZeroRect
          pullsDown:NO
              model:&model]);
  [button setFont:LabelFont()];

  if (best_matched_username_index < forms.size()) {
    [button selectItemAtIndex:best_matched_username_index];
  } else if (preffered_form_index < forms.size()) {
    [button selectItemAtIndex:preffered_form_index];
  } else {
    [button selectItemAtIndex:0];
  }

  [button sizeToFit];
  return button.autorelease();
}

}  // namespace

@implementation CredentialsSelectionView

- (id)initWithModel:(ManagePasswordsBubbleModel*)model {
  if ((self = [super init])) {
    model_ = model;

    // Create the pop up button with usernames and the password field.
    usernamePopUpButton_.reset([CreateUsernamesPopUpButton(
        model_->local_credentials(),
        model_->pending_password().username_value) retain]);
    passwordField_.reset(
        [PasswordLabel(model_->pending_password().password_value) retain]);

    // Calculate desired widths of username and password.
    CGFloat firstWidth = NSMaxX([usernamePopUpButton_ frame]);
    CGFloat secondWidth = NSMaxX([passwordField_ frame]);
    std::pair<CGFloat, CGFloat> sizes = GetResizedColumns(
        kDesiredRowWidth, std::make_pair(firstWidth, secondWidth));
    CGFloat curX = 0;
    CGFloat curY = 0;

    [usernamePopUpButton_
        setFrameSize:NSMakeSize(sizes.first,
                                NSHeight([usernamePopUpButton_ frame]))];
    [self addSubview:usernamePopUpButton_];

    [passwordField_ setFrameSize:NSMakeSize(sizes.second,
                                            NSHeight([passwordField_ frame]))];
    [self addSubview:passwordField_];

    // Calculate username and password positions. The element with bigger height
    // will have Y coordinate equal to curY, another one will have Y coordinate
    // equals to curY + half of their heights difference.
    CGFloat popUpLabelHeightDifference =
        NSHeight([usernamePopUpButton_ frame]) -
        NSHeight([passwordField_ frame]);

    CGFloat usernameY = std::max(curY, curY - popUpLabelHeightDifference / 2);
    CGFloat passwordY = std::max(curY, curY + popUpLabelHeightDifference / 2);

    [usernamePopUpButton_ setFrameOrigin:NSMakePoint(curX, usernameY)];
    // Move to the right of the username and add the password.
    curX = NSMaxX([usernamePopUpButton_ frame]) + kItemLabelSpacing;
    [passwordField_ setFrameOrigin:NSMakePoint(curX, passwordY)];

    // Move to the top-right of the password.
    curY = std::max(NSMaxY([passwordField_ frame]),
                    NSMaxY([usernamePopUpButton_ frame]));

    // Update the frame.
    [self setFrameSize:NSMakeSize(kDesiredRowWidth, curY)];
  }
  return self;
}

- (const autofill::PasswordForm*)getSelectedCredentials {
  int selected_index = [usernamePopUpButton_ indexOfSelectedItem];
  CHECK(selected_index >= 0);
  return &model_->local_credentials()[selected_index];
}

@end
