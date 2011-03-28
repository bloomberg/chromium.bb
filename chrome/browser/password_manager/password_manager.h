// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/ui/login/login_model.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

class PasswordManagerDelegate;
class PasswordManagerTest;
class PasswordFormManager;
class PrefService;

// Per-tab password manager. Handles creation and management of UI elements,
// receiving password form data from the renderer and managing the password
// database through the WebDataService. The PasswordManager is a LoginModel
// for purposes of supporting HTTP authentication dialogs.
class PasswordManager : public LoginModel,
                        public TabContentsObserver {
 public:
  static void RegisterUserPrefs(PrefService* prefs);

  // The delegate passed in is required to outlive the PasswordManager.
  PasswordManager(TabContents* tab_contents,
                  PasswordManagerDelegate* delegate);
  virtual ~PasswordManager();

  // Called by a PasswordFormManager when it decides a form can be autofilled
  // on the page.
  void Autofill(const webkit_glue::PasswordForm& form_for_autofill,
                const webkit_glue::PasswordFormMap& best_matches,
                const webkit_glue::PasswordForm* const preferred_match,
                bool wait_for_username) const;

  // LoginModel implementation.
  virtual void SetObserver(LoginModelObserver* observer);

  // When a form is submitted, we prepare to save the password but wait
  // until we decide the user has successfully logged in. This is step 1
  // of 2 (see SavePassword).
  void ProvisionallySavePassword(webkit_glue::PasswordForm form);

  // TabContentsObserver overrides.
  virtual void DidStopLoading();
  virtual void DidNavigateAnyFramePostCommit(
      const NavigationController::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnPasswordFormsFound(
      const std::vector<webkit_glue::PasswordForm>& forms);
  void OnPasswordFormsVisible(
      const std::vector<webkit_glue::PasswordForm>& visible_forms);

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordManagerTest, FormSeenThenLeftPage);

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

  // Clear any pending saves
  void ClearProvisionalSave();

  // Notification that the user navigated away from the current page.
  // Unless this is a password form submission, for our purposes this
  // means we're done with the current page, so we can clean-up.
  void DidNavigate();

  typedef std::vector<PasswordFormManager*> LoginManagers;
  LoginManagers pending_login_managers_;

  // Deleter for pending_login_managers_ when PasswordManager is deleted (e.g
  // tab closes) on a page with a password form, thus containing login managers.
  STLElementDeleter<LoginManagers> login_managers_deleter_;

  // When the user submits a password/credential, this contains the
  // PasswordFormManager for the form in question until we deem the login
  // attempt to have succeeded (as in valid credentials). If it fails, we
  // send the PasswordFormManager back to the pending_login_managers_ set.
  // Scoped in case PasswordManager gets deleted (e.g tab closes) between the
  // time a user submits a login form and gets to the next page.
  scoped_ptr<PasswordFormManager> provisional_save_manager_;

  // Our delegate for carrying out external operations.  This is typically the
  // containing TabContents.
  PasswordManagerDelegate* delegate_;

  // The LoginModelObserver (i.e LoginView) requiring autofill.
  LoginModelObserver* observer_;

  // Set to false to disable the password manager (will no longer fill
  // passwords or ask you if you want to save passwords).
  BooleanPrefMember password_manager_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PasswordManager);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_H_
