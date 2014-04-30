// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_

#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents_observer.h"

class ManagePasswordsIconController;

namespace content {
class WebContents;
}

// This model provides data for the ManagePasswordsBubble and controls the
// password management actions.
class ManagePasswordsBubbleModel : public content::WebContentsObserver {
 public:
  enum ManagePasswordsBubbleState {
    PASSWORD_TO_BE_SAVED,
    MANAGE_PASSWORDS,
    NEVER_SAVE_PASSWORDS,
  };

  enum PasswordAction { REMOVE_PASSWORD, ADD_PASSWORD };

  // Creates a ManagePasswordsBubbleModel, which holds a raw pointer to the
  // WebContents in which it lives. Defaults to a display disposition of
  // AUTOMATIC_WITH_PASSWORD_PENDING, and a dismissal reason of NOT_DISPLAYED.
  // The bubble's state is updated from the ManagePasswordsBubbleUIController
  // associated with |web_contents| upon creation.
  explicit ManagePasswordsBubbleModel(content::WebContents* web_contents);
  virtual ~ManagePasswordsBubbleModel();

  // Called by the view code when the bubble is shown.
  void OnBubbleShown(ManagePasswordsBubble::DisplayReason reason);

  // Called by the view code when the bubble is hidden.
  void OnBubbleHidden();

  // Called by the view code when the "Nope" button in clicked by the user.
  void OnNopeClicked();

  // Called by the view code when the "Never for this site." button in clicked
  // by the user.
  void OnNeverForThisSiteClicked();

  // Called by the view code when the save button in clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the "Done" button is clicked by the user.
  void OnDoneClicked();

  // Called by the view code when the manage link is clicked by the user.
  void OnManageLinkClicked();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const autofill::PasswordForm& password_form,
                        PasswordAction action);

  // Called by the view code when the bubble is closed without ever displaying
  // content to the user. We shouldn't log to UMA in this case.
  void OnCloseWithoutLogging();

  ManagePasswordsBubbleState manage_passwords_bubble_state() {
    return manage_passwords_bubble_state_;
  }

  bool WaitingToSavePassword() {
    return manage_passwords_bubble_state() == PASSWORD_TO_BE_SAVED;
  }

  const base::string16& title() { return title_; }
  const autofill::PasswordForm& pending_credentials() {
    return pending_credentials_;
  }
  const autofill::PasswordFormMap& best_matches() { return best_matches_; }
  const base::string16& manage_link() { return manage_link_; }

  // Gets and sets the reason the bubble was displayed; exposed for testing.
  password_manager::metrics_util::UIDisplayDisposition display_disposition()
      const {
    return display_disposition_;
  }

  // Gets the reason the bubble was dismissed; exposed for testing.
  password_manager::metrics_util::UIDismissalReason dismissal_reason() const {
    return dismissal_reason_;
  }

  // State setter; exposed for testing.
  void set_manage_passwords_bubble_state(ManagePasswordsBubbleState state) {
    manage_passwords_bubble_state_ = state;
  }

 private:
  // content::WebContentsObserver
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  content::WebContents* web_contents_;
  ManagePasswordsBubbleState manage_passwords_bubble_state_;
  base::string16 title_;
  autofill::PasswordForm pending_credentials_;
  autofill::PasswordFormMap best_matches_;
  base::string16 manage_link_;

  password_manager::metrics_util::UIDisplayDisposition display_disposition_;
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
