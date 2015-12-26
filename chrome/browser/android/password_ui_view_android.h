// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_

#include <stddef.h>

#include <vector>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/password_manager_presenter.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

// PasswordUIView for Android, contains jni hooks that allows Android UI to
// display passwords and route UI commands back to native
// PasswordManagerPresenter.
class PasswordUIViewAndroid : public PasswordUIView {
 public:
  PasswordUIViewAndroid(JNIEnv* env, jobject);
  ~PasswordUIViewAndroid() override;

  // PasswordUIView implementation.
  Profile* GetProfile() override;
  void ShowPassword(
      size_t index,
      const std::string& origin_url,
      const std::string& username,
      const base::string16& password_value) override;
  void SetPasswordList(
      const std::vector<scoped_ptr<autofill::PasswordForm>>& password_list,
      bool show_passwords) override;
  void SetPasswordExceptionList(
      const std::vector<scoped_ptr<autofill::PasswordForm>>&
          password_exception_list) override;

  // Calls from Java.
  base::android::ScopedJavaLocalRef<jobject> GetSavedPasswordEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      int index);
  base::android::ScopedJavaLocalRef<jstring> GetSavedPasswordException(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      int index);
  void UpdatePasswordLists(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>&);
  void HandleRemoveSavedPasswordEntry(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      int index);
  void HandleRemoveSavedPasswordException(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>&,
      int index);
  // Destroy the native implementation.
  void Destroy(JNIEnv*, const base::android::JavaParamRef<jobject>&);

  // JNI registration
  static bool RegisterPasswordUIViewAndroid(JNIEnv* env);

 private:
  PasswordManagerPresenter password_manager_presenter_;
  // Java side of UI controller.
  JavaObjectWeakGlobalRef weak_java_ui_controller_;

  DISALLOW_COPY_AND_ASSIGN(PasswordUIViewAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PASSWORD_UI_VIEW_ANDROID_H_
