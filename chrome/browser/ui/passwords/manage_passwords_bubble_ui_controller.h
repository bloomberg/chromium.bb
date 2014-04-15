// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_

#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Per-tab class to control the Omnibox password icon and bubble.
class ManagePasswordsBubbleUIController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManagePasswordsBubbleUIController>,
      public password_manager::PasswordStore::Observer {
 public:
  virtual ~ManagePasswordsBubbleUIController();

  // Called when the user submits a form containing login information, so we
  // can handle later requests to save or blacklist that login information.
  // This stores the provided object in form_manager_ and triggers the UI to
  // prompt the user about whether they would like to save the password.
  void OnPasswordSubmitted(password_manager::PasswordFormManager* form_manager);

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_form_map|. This stores a copy of |password_form_map| and shows
  // the manage password icon.
  void OnPasswordAutofilled(const autofill::PasswordFormMap& password_form_map);

  // Called when a form is _not_ autofilled due to user blacklisting.
  void OnBlacklistBlockedAutofill();

  // PasswordStore::Observer implementation.
  virtual void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) OVERRIDE;

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

  const autofill::PasswordForm& pending_credentials() const {
    return form_manager_->pending_credentials();
  }

  const autofill::PasswordFormMap best_matches() const {
    return password_form_map_;
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
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // Set by OnPasswordSubmitted() when the user submits a form containing login
  // information.  If the user responds to a subsequent "Do you want to save
  // this password?" prompt, we ask this object to save or blacklist the
  // associated login information in Chrome's password store.
  scoped_ptr<password_manager::PasswordFormManager> form_manager_;

  // All previously stored credentials for a specific site.  Set by
  // OnPasswordSubmitted() or OnPasswordAutofilled().
  autofill::PasswordFormMap password_form_map_;

  bool manage_passwords_icon_to_be_shown_;
  bool password_to_be_saved_;
  bool manage_passwords_bubble_needs_showing_;

  // Stores whether autofill was blocked due to a user's decision to blacklist
  // the current site ("Never save passwords for this site").
  bool autofill_blocked_;

  // The origin of the form we're currently dealing with; we'll use this to
  // determine which PasswordStore changes we should care about when updating
  // |password_form_map_|.
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleUIController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
