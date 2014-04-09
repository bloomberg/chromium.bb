// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_

#include "components/password_manager/core/browser/password_form_manager.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Per-tab class to control the Omnibox password icon and bubble.
class ManagePasswordsBubbleUIController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsBubbleUIController> {
 public:
  virtual ~ManagePasswordsBubbleUIController();

  // Called when the user submits a form containing login information, so we
  // can handle later requests to save or blacklist that login information.
  // This stores the provided object in form_manager_ and triggers the UI to
  // prompt the user about whether they would like to save the password.
  void OnPasswordSubmitted(PasswordFormManager* form_manager);

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_form_map|. This stores a copy of |password_form_map| and shows
  // the manage password icon.
  void OnPasswordAutofilled(const autofill::PasswordFormMap& password_form_map);

  // Called when a form is _not_ autofilled due to user blacklisting.
  void OnBlacklistBlockedAutofill();

  // TODO(npentrel) This ought to be changed. Best matches should be newly
  // made when opening the ManagePasswordsBubble because there may have been
  // changes to the best matches via the settings page. At the moment this also
  // fails if one deletes a password when they are autofilled, as it still shows
  // up after logging in and saving a password.
  void RemoveFromBestMatches(autofill::PasswordForm password_form);

  void SavePassword();

  void NeverSavePassword();

  // Called when the bubble is opened after the icon gets displayed. We change
  // the state to know that we do not need to pop up the bubble again.
  void OnBubbleShown();

  bool manage_passwords_icon_to_be_shown() const {
    return manage_passwords_icon_to_be_shown_;
  }

  bool password_to_be_saved() const {
    return password_to_be_saved_;
  }

  bool manage_passwords_bubble_needs_showing() const {
    return manage_passwords_bubble_needs_showing_;
  }

  void unset_manage_passwords_bubble_needs_showing() {
    manage_passwords_bubble_needs_showing_ = false;
  }

  void unset_password_to_be_saved() {
    password_to_be_saved_ = false;
  }

  const autofill::PasswordForm pending_credentials() const {
    return form_manager_->pending_credentials();
  }

  const autofill::PasswordFormMap best_matches() const {
    return password_form_map_;
  }

  bool password_submitted() const {
    return password_submitted_;
  }

  void set_password_submitted(bool password_submitted) {
    password_submitted_ = password_submitted;
  }

  bool autofill_blocked() const { return autofill_blocked_; }
  void set_autofill_blocked(bool autofill_blocked) {
    autofill_blocked_ = autofill_blocked;
  }

 private:
  friend class content::WebContentsUserData<ManagePasswordsBubbleUIController>;

  explicit ManagePasswordsBubbleUIController(
      content::WebContents* web_contents);

  // Called when a passwordform is autofilled, when a new passwordform is
  // submitted, or when a navigation occurs to update the visibility of the
  // manage passwords icon and bubble.
  void UpdateBubbleAndIconVisibility();

  // content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Set by OnPasswordSubmitted() when the user submits a form containing login
  // information.  If the user responds to a subsequent "Do you want to save
  // this password?" prompt, we ask this object to save or blacklist the
  // associated login information in Chrome's password store.
  scoped_ptr<PasswordFormManager> form_manager_;

  // All previously stored credentials for a specific site.  Set by
  // OnPasswordSubmitted() or OnPasswordAutofilled().
  autofill::PasswordFormMap password_form_map_;

  bool manage_passwords_icon_to_be_shown_;
  bool password_to_be_saved_;
  bool manage_passwords_bubble_needs_showing_;
  // Stores whether a new password has been submitted, if so we have
  // |pending_credentials|.
  bool password_submitted_;

  // Stores whether autofill was blocked due to a user's decision to blacklist
  // the current site ("Never save passwords for this site").
  bool autofill_blocked_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleUIController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
