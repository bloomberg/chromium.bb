// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_

#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_store.h"
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
      public PasswordStore::Observer,
      public PasswordManager::PasswordObserver {
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

  // Called when the credentials in the |password_form_manager_| are updated to
  // update the local |best_matches_|.
  void OnBestMatchesUpdated();

  void SavePassword();

  // Called when the bubble is opened after the icon gets displayed. We change
  // the state to know that we do not need to pop up the bubble again.
  void OnBubbleShown();

  // Called by the view code to delete or add a password form to the
  // PasswordStore.
  void OnCredentialAction(autofill::PasswordForm password_form, bool remove);

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

  const autofill::PasswordForm pending_credentials() const {
    return pending_credentials_;
  }

  const autofill::PasswordFormMap best_matches() const {
    return password_form_map_;
  }

 private:
  friend class content::WebContentsUserData<ManagePasswordsBubbleUIController>;

  explicit ManagePasswordsBubbleUIController(
      content::WebContents* web_contents);

  // ManagePasswordsBubbleUIController::PasswordObserver:
  virtual void OnPasswordAction(
      PasswordObserver::BubbleNotification notification,
      const autofill::PasswordFormMap& best_matches,
      const autofill::PasswordForm& pending_credentials) OVERRIDE;
  virtual void OnPasswordsUpdated(
      const autofill::PasswordFormMap& best_matches) OVERRIDE;

  // Returns the password store associated with the web_contents.
  PasswordStore* GetPasswordStore(content::WebContents* web_contents);

  // Called when something in the PasswordStore changes or when the tab is
  // changed, since the logins could have changed on a different tab.
  void UpdateBestMatches();

  // Called when a PasswordForm is autofilled, when a new passwordform is
  // submitted, or when a navigation occurs to update the visibility of the
  // manage passwords icon and bubble.
  void UpdateBubbleAndIconVisibility();

  // content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  // PasswordStore::Observer:
  virtual void OnLoginsChanged() OVERRIDE;

  // All previously stored credentials for a specific site.  Set by
  // OnPasswordSubmitted() or OnPasswordAutofilled().
  autofill::PasswordFormMap password_form_map_;

  autofill::PasswordForm pending_credentials_;

  // Saves a password form such that it can be used in UpdateBestMatches().
  autofill::PasswordForm observed_form_;

  bool manage_passwords_icon_to_be_shown_;
  bool password_to_be_saved_;
  bool manage_passwords_bubble_needs_showing_;

  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleUIController);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_BUBBLE_UI_CONTROLLER_H_
