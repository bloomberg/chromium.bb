// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_member.h"
#include "base/stl_util.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/ui/login/login_model.h"
#include "components/autofill/common/password_form_fill_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/password_form.h"

class PasswordManagerDelegate;
class PasswordManagerTest;
class PasswordFormManager;
class PrefRegistrySyncable;

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the PasswordStore. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public LoginModel,
                        public content::WebContentsObserver,
                        public content::WebContentsUserData<PasswordManager> {
 public:
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  static void CreateForWebContentsAndDelegate(
      content::WebContents* contents,
      PasswordManagerDelegate* delegate);
  virtual ~PasswordManager();

  // Is saving new data for password autofill enabled for the current profile?
  // For example, saving new data is disabled in Incognito mode, whereas filling
  // data is not.
  bool IsSavingEnabled() const;

  // Called by a PasswordFormManager when it decides a form can be autofilled
  // on the page.
  virtual void Autofill(const content::PasswordForm& form_for_autofill,
                        const content::PasswordFormMap& best_matches,
                        const content::PasswordForm& preferred_match,
                        bool wait_for_username) const;

  // LoginModel implementation.
  virtual void SetObserver(LoginModelObserver* observer) OVERRIDE;

  // Mark this form as having a generated password.
  void SetFormHasGeneratedPassword(const content::PasswordForm& form);

  // TODO(isherman): This should not be public, but is currently being used by
  // the LoginPrompt code.
  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  void ProvisionallySavePassword(const content::PasswordForm& form);

  // content::WebContentsObserver overrides.
  virtual void DidNavigateAnyFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // TODO(isherman): This should not be public, but is currently being used by
  // the LoginPrompt code.
  void OnPasswordFormsParsed(
      const std::vector<content::PasswordForm>& forms);
  void OnPasswordFormsRendered(
      const std::vector<content::PasswordForm>& visible_forms);

 protected:
  // Subclassed for unit tests.
  PasswordManager(content::WebContents* web_contents,
                  PasswordManagerDelegate* delegate);

 private:
  friend class content::WebContentsUserData<PasswordManager>;

  // Note about how a PasswordFormManager can transition from
  // pending_login_managers_ to provisional_save_manager_ and the infobar.
  //
  // 1. form "seen"
  //       |                                             new
  //       |                                               ___ Infobar
  // pending_login -- form submit --> provisional_save ___/
  //             ^                            |           \___ (update DB)
  //             |                           fail
  //             |-----------<------<---------|          !new
  //
  // When a form is "seen" on a page, a PasswordFormManager is created
  // and stored in this collection until user navigates away from page.

  ScopedVector<PasswordFormManager> pending_login_managers_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  scoped_ptr<PasswordFormManager> provisional_save_manager_;

  // Our delegate for carrying out external operations.  This is typically the
  // containing WebContents.
  PasswordManagerDelegate* const delegate_;

  // The LoginModelObserver (i.e LoginView) requiring autofill.
  LoginModelObserver* observer_;

  // Set to false to disable the password manager (will no longer ask if you
  // want to save passwords but will continue to fill passwords).
  BooleanPrefMember password_manager_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_
