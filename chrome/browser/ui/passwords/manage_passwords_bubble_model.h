// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

class PasswordsModelDelegate;
class Profile;

// This model provides data for the ManagePasswordsBubble and controls the
// password management actions.
class ManagePasswordsBubbleModel {
 public:
  enum PasswordAction { REMOVE_PASSWORD, ADD_PASSWORD };
  enum DisplayReason { AUTOMATIC, USER_ACTION };

  // Creates a ManagePasswordsBubbleModel, which holds a weak pointer to the
  // delegate. Construction implies that the bubble is shown. The bubble's state
  // is updated from the ManagePasswordsUIController associated with |delegate|.
  ManagePasswordsBubbleModel(base::WeakPtr<PasswordsModelDelegate> delegate,
                             DisplayReason reason);
  ~ManagePasswordsBubbleModel();

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

  // Called by the view code when the navigate to passwords.google.com link is
  // clicked by the user.
  void OnNavigateToPasswordManagerAccountDashboardLinkClicked();

  // Called by the view code when the brand name link is clicked by the user.
  void OnBrandLinkClicked();

  // Called by the view code when the auto-signin toast is about to close due to
  // timeout.
  void OnAutoSignInToastTimeout();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnPasswordAction(const autofill::PasswordForm& password_form,
                        PasswordAction action);

  // Called by the view when the "Sign in" button in the promo bubble is
  // clicked.
  void OnSignInToChromeClicked();

  // Called by the view when the "No thanks" button in the promo bubble is
  // clicked.
  void OnSkipSignInClicked();

  password_manager::ui::State state() const { return state_; }

  const base::string16& title() const { return title_; }
  const autofill::PasswordForm& pending_password() const {
    return pending_password_;
  }
  // Returns the available credentials which match the current site.
  const std::vector<autofill::PasswordForm>& local_credentials() const {
    return local_credentials_;
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

  Profile* GetProfile() const;
  content::WebContents* GetWebContents() const;

  // Returns true iff the multiple account selection prompt for account update
  // should be presented.
  bool ShouldShowMultipleAccountUpdateUI() const;

  // Returns true and updates the internal state iff the Save bubble should
  // switch to show a promotion after the password was saved. Otherwise,
  // returns false and leaves the current state.
  bool ReplaceToShowPromotionIfNeeded();

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

 private:
  enum UserBehaviorOnUpdateBubble {
    UPDATE_CLICKED,
    NOPE_CLICKED,
    NO_INTERACTION
  };
  class InteractionKeeper;
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
  std::vector<autofill::PasswordForm> local_credentials_;
  base::string16 manage_link_;
  base::string16 save_confirmation_text_;
  gfx::Range save_confirmation_link_range_;

  // Responsible for recording all the interactions required.
  std::unique_ptr<InteractionKeeper> interaction_keeper_;

  // A bridge to ManagePasswordsUIController instance.
  base::WeakPtr<PasswordsModelDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleModel);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_MODEL_H_
