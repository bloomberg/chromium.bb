// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_

#include "components/autofill/core/common/password_form.h"

class ManagePasswordsIconController;

namespace content {
class WebContents;
}

// This model provides data for the ManagePasswordsBubble and controls the
// password management actions.
class ManagePasswordsBubbleModel {
 public:
  explicit ManagePasswordsBubbleModel(content::WebContents* web_contents);
  virtual ~ManagePasswordsBubbleModel();

  enum ManagePasswordsBubbleState {
    PASSWORD_TO_BE_SAVED,
    MANAGE_PASSWORDS
  };

  // Called by the view code when the cancel button in clicked by the user.
  void OnCancelClicked();

  // Called by the view code when the save button in clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the manage link is clicked by the user.
  void OnManageLinkClicked();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnCredentialAction(autofill::PasswordForm password_form, bool remove);

  ManagePasswordsBubbleState manage_passwords_bubble_state() {
    return manage_passwords_bubble_state_;
  }

  const base::string16& title() { return title_; }
  const autofill::PasswordForm& pending_credentials() {
    return pending_credentials_;
  }
  const autofill::PasswordFormMap& best_matches() { return best_matches_; }
  const base::string16& manage_link() { return manage_link_; }

 private:
  content::WebContents* web_contents_;
  ManagePasswordsBubbleState manage_passwords_bubble_state_;
  base::string16 title_;
  autofill::PasswordForm pending_credentials_;
  autofill::PasswordFormMap best_matches_;
  base::string16 manage_link_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
