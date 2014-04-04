// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_

#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/web_contents_observer.h"

class ManagePasswordsIconController;

namespace content {
class WebContents;
}

// This model provides data for the ManagePasswordsBubble and controls the
// password management actions.
class ManagePasswordsBubbleModel : public content::WebContentsObserver {
 public:
  explicit ManagePasswordsBubbleModel(content::WebContents* web_contents);
  virtual ~ManagePasswordsBubbleModel();

  enum ManagePasswordsBubbleState {
    PASSWORD_TO_BE_SAVED,
    MANAGE_PASSWORDS_AFTER_SAVING,
    MANAGE_PASSWORDS,
    NEVER_SAVE_PASSWORDS,
  };

  enum PasswordAction { REMOVE_PASSWORD, ADD_PASSWORD };

  // Called by the view code when the "Nope" button in clicked by the user.
  void OnNopeClicked();

  // Called by the view code when the "Never for this site." button in clicked
  // by the user.
  void OnNeverForThisSiteClicked();

  // Called by the view code when the save button in clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the manage link is clicked by the user.
  void OnManageLinkClicked();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const autofill::PasswordForm& password_form,
                        PasswordAction action);

  // Called by the view code when the ManagePasswordItemView is destroyed and
  // the user chose to delete the password.
  // TODO(npentrel): Remove this once best_matches_ are newly made on bubble
  // opening.
  void DeleteFromBestMatches(autofill::PasswordForm password_form);

  ManagePasswordsBubbleState manage_passwords_bubble_state() {
    return manage_passwords_bubble_state_;
  }

  bool WaitingToSavePassword() {
    return manage_passwords_bubble_state() == PASSWORD_TO_BE_SAVED;
  }

  bool password_submitted() { return password_submitted_; }
  const base::string16& title() { return title_; }
  const autofill::PasswordForm& pending_credentials() {
    return pending_credentials_;
  }
  const autofill::PasswordFormMap& best_matches() { return best_matches_; }
  const base::string16& manage_link() { return manage_link_; }

 private:
  // content::WebContentsObserver
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  content::WebContents* web_contents_;
  ManagePasswordsBubbleState manage_passwords_bubble_state_;
  bool password_submitted_;
  base::string16 title_;
  autofill::PasswordForm pending_credentials_;
  autofill::PasswordFormMap best_matches_;
  base::string16 manage_link_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
