// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/autofill/core/common/password_form.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialInfo;
class PasswordFormManager;
}

// An interface for ChromePasswordManagerClient implemented by
// ManagePasswordsUIController. Allows to push a new state for the tab.
class PasswordsClientUIDelegate {
 public:
  // Called when the user submits a form containing login information, so the
  // later requests to save or blacklist can be handled.
  // This stores the provided object and triggers the UI to prompt the user
  // about whether they would like to save the password.
  virtual void OnPasswordSubmitted(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) = 0;

  // Called when the user submits a new password for an existing credential.
  // This stores the provided object and triggers the UI to prompt the user
  // about whether they would like to update the password.
  virtual void OnUpdatePasswordSubmitted(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) = 0;

  // Called when the site asks user to choose from credentials. This triggers
  // the UI to prompt the user. |local_credentials| and |federated_credentials|
  // shouldn't both be empty.
  // Returns true when the UI is shown. |callback| is called when the user made
  // a decision. If the UI isn't shown the method returns false and doesn't call
  // |callback|.
  virtual bool OnChooseCredentials(
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin,
      base::Callback<void(const password_manager::CredentialInfo&)>
          callback) = 0;

  // Called when user is auto signed in to the site. |local_forms[0]| contains
  // the credential returned to the site.
  virtual void OnAutoSignin(
      ScopedVector<autofill::PasswordForm> local_forms) = 0;

  // Called when it's the right time to enable autosign-in explicitly.
  virtual void OnPromptEnableAutoSignin() = 0;

  // Called when the password will be saved automatically, but we still wish to
  // visually inform the user that the save has occured.
  virtual void OnAutomaticPasswordSave(
      scoped_ptr<password_manager::PasswordFormManager> form_manager) = 0;

  // Called when a form is autofilled with login information, so we can manage
  // password credentials for the current site which are stored in
  // |password_form_map|. This stores a copy of |password_form_map| and shows
  // the manage password icon.
  virtual void OnPasswordAutofilled(
      const autofill::PasswordFormMap& password_form_map,
      const GURL& origin) = 0;

 protected:
  virtual ~PasswordsClientUIDelegate() = default;
};

// Returns ManagePasswordsUIController instance for |contents|
PasswordsClientUIDelegate* PasswordsClientUIDelegateFromWebContents(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORDS_CLIENT_UI_DELEGATE_H_
