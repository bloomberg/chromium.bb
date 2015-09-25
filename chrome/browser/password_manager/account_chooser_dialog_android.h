// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_

#include <vector>

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

namespace content {
class WebContents;
}

// Native counterpart for the android dialog which allows users to select
// credentials which will be passed to the web site in order to log in the user.
class AccountChooserDialogAndroid {
 public:
  AccountChooserDialogAndroid(content::WebContents* web_contents,
                              ManagePasswordsUIController* ui_controller);
  ~AccountChooserDialogAndroid();
  void Destroy(JNIEnv* env, jobject obj);

  void ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  void CancelDialog(JNIEnv* env, jobject obj);

  // Propagates the credentials chosen by the user.
  void OnCredentialClicked(JNIEnv* env,
                           jobject obj,
                           jint credential_item,
                           jint credential_type);

  // Opens new tab with page which explains the Smart Lock branding.
  void OnLinkClicked(JNIEnv* env, jobject obj);

 private:
  const std::vector<const autofill::PasswordForm*>& local_credentials_forms()
      const {
    return ui_controller_->GetCurrentForms();
  }

  const std::vector<const autofill::PasswordForm*>&
  federated_credentials_forms() const {
    return ui_controller_->GetFederatedForms();
  }

  void ChooseCredential(size_t index, password_manager::CredentialType type);

  content::WebContents* web_contents_;
  ManagePasswordsUIController* ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserDialogAndroid);
};

// Native JNI methods
bool RegisterAccountChooserDialogAndroid(JNIEnv* env);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
