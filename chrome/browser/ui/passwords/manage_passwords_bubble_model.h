// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_

#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/range/range.h"

class ManagePasswordsIconController;
class ManagePasswordsUIController;
class Profile;

namespace content {
class WebContents;
}

namespace password_manager {
enum class CredentialType;
}

// This model provides data for the ManagePasswordsBubble and controls the
// password management actions.
class ManagePasswordsBubbleModel : public content::WebContentsObserver {
 public:
  enum PasswordAction { REMOVE_PASSWORD, ADD_PASSWORD };
  enum DisplayReason { AUTOMATIC, USER_ACTION };

  // Creates a ManagePasswordsBubbleModel, which holds a raw pointer to the
  // WebContents in which it lives. Construction implies that the bubble
  // is shown. The bubble's state is updated from the
  // ManagePasswordsUIController associated with |web_contents|.
  ManagePasswordsBubbleModel(content::WebContents* web_contents,
                             DisplayReason reason);
  ~ManagePasswordsBubbleModel() override;

  // Called by the view code when the "Cancel" button in clicked by the user.
  void OnCancelClicked();

  // Called by the view code when the "Nope" button in clicked by the user in
  // update bubble.
  void OnNopeUpdateClicked();

  // Called by the view code when the "Never for this site." button in clicked
  // by the user.
  void OnNeverForThisSiteClicked();

  // Called by the view code when the save button is clicked by the user.
  void OnSaveClicked();

  // Called by the view code when the update link is clicked by the user.
  void OnUpdateClicked(const autofill::PasswordForm& password_form);

  // Called by the view code when the "Done" button is clicked by the user.
  void OnDoneClicked();

  // Called by the view code when the "OK" button is clicked by the user.
  void OnOKClicked();

  // Called by the view code when the manage link is clicked by the user.
  void OnManageLinkClicked();

  // Called by the view code when the brand name link is clicked by the user.
  void OnBrandLinkClicked();

  // Called by the view code when the auto-signin toast is about to close due to
  // timeout.
  void OnAutoSignInToastTimeout();

  // Called by the view code when the user clicks "Ok, got it".
  void OnAutoSignOKClicked();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const autofill::PasswordForm& password_form,
                        PasswordAction action);

  // Called by the view code to notify about chosen credential.
  void OnChooseCredentials(const autofill::PasswordForm& password_form,
                           password_manager::CredentialType credential_type_);

  GURL origin() const { return origin_; }

  password_manager::ui::State state() const { return state_; }

  const base::string16& title() const { return title_; }
  const autofill::PasswordForm& pending_password() const {
    return pending_password_;
  }
  // Returns the available credentials which match the current site.
  const ScopedVector<const autofill::PasswordForm>& local_credentials() const {
    return local_credentials_;
  }
  // Return the federated logins which may be used for logging in to the current
  // site.
  const ScopedVector<const autofill::PasswordForm>& federated_credentials()
      const {
    return federated_credentials_;
  }
  const base::string16& manage_link() const { return manage_link_; }
  const base::string16& save_confirmation_text() const {
    return save_confirmation_text_;
  }
  const gfx::Range& save_confirmation_link_range() const {
    return save_confirmation_link_range_;
  }

  const gfx::Range& title_brand_link_range() const {
    return title_brand_link_range_;
  }

  const base::string16& autosignin_welcome_text() const {
    return autosignin_welcome_text_;
  }

  const gfx::Range& autosignin_welcome_link_range() const {
    return autosignin_welcome_link_range_;
  }

  Profile* GetProfile() const;

  // Returns true iff the multiple account selection prompt for account update
  // should be presented.
  bool ShouldShowMultipleAccountUpdateUI() const;

  // True if the save bubble should display the warm welcome for Google Smart
  // Lock.
  bool ShouldShowGoogleSmartLockWelcome() const;

  // True if the autosignin toast should display the warm welcome message.
  bool ShouldShowAutoSigninWarmWelcome() const;

#if defined(UNIT_TEST)
  // Gets and sets the reason the bubble was displayed.
  password_manager::metrics_util::UIDisplayDisposition display_disposition()
      const {
    return display_disposition_;
  }

  // Gets the reason the bubble was dismissed.
  password_manager::metrics_util::UIDismissalReason dismissal_reason() const {
    return dismissal_reason_;
  }

  // State setter.
  void set_state(password_manager::ui::State state) { state_ = state; }
#endif

 private:
  enum UserBehaviorOnUpdateBubble {
    UPDATE_CLICKED,
    NOPE_CLICKED,
    NO_INTERACTION
  };
  // Updates |title_| and |title_brand_link_range_| for the
  // PENDING_PASSWORD_STATE.
  void UpdatePendingStateTitle();
  // Updates |title_| for the MANAGE_STATE.
  void UpdateManageStateTitle();
  password_manager::metrics_util::UpdatePasswordSubmissionEvent
  GetUpdateDismissalReason(UserBehaviorOnUpdateBubble behavior) const;
  // URL of the page from where this bubble was triggered.
  GURL origin_;
  password_manager::ui::State state_;
  base::string16 title_;
  // Range of characters in the title that contains the Smart Lock Brand and
  // should point to an article. For the default title the range is empty.
  gfx::Range title_brand_link_range_;
  autofill::PasswordForm pending_password_;
  bool password_overridden_;
  ScopedVector<const autofill::PasswordForm> local_credentials_;
  ScopedVector<const autofill::PasswordForm> federated_credentials_;
  base::string16 manage_link_;
  base::string16 save_confirmation_text_;
  gfx::Range save_confirmation_link_range_;
  base::string16 autosignin_welcome_text_;
  gfx::Range autosignin_welcome_link_range_;
  password_manager::metrics_util::UIDisplayDisposition display_disposition_;
  password_manager::metrics_util::UIDismissalReason dismissal_reason_;
  password_manager::metrics_util::UpdatePasswordSubmissionEvent
      update_password_submission_event_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
