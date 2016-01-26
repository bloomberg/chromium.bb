// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_

#include <stddef.h>

#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/passwords/manage_passwords_state.h"

namespace content {
class WebContents;
}

namespace password_manager {
struct CredentialInfo;
}

// Native counterpart for the android dialog which allows users to select
// credentials which will be passed to the web site in order to log in the user.
class AccountChooserDialogAndroid {
 public:
  AccountChooserDialogAndroid(
      content::WebContents* web_contents,
      ScopedVector<autofill::PasswordForm> local_credentials,
      ScopedVector<autofill::PasswordForm> federated_credentials,
      const GURL& origin,
      const ManagePasswordsState::CredentialsCallback& callback);

  ~AccountChooserDialogAndroid();
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void ShowDialog();

  // Closes the dialog and propagates that no credentials was chosen.
  void CancelDialog(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);

  // Propagates the credentials chosen by the user.
  void OnCredentialClicked(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint credential_item,
                           jint credential_type);

  // Opens new tab with page which explains the Smart Lock branding.
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

 private:
  const std::vector<const autofill::PasswordForm*>& local_credentials_forms()
      const;

  const std::vector<const autofill::PasswordForm*>&
  federated_credentials_forms() const;

  void ChooseCredential(size_t index, password_manager::CredentialType type);

  content::WebContents* web_contents_;
  ManagePasswordsState passwords_data_;
  GURL origin_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserDialogAndroid);
};

// Native JNI methods
bool RegisterAccountChooserDialogAndroid(JNIEnv* env);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_CHOOSER_DIALOG_ANDROID_H_
