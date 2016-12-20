// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_ui_view_android.h"

#include <algorithm>

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "jni/PasswordUIView_jni.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

// Returns the human readable version of the origin string displayed in
// Chrome settings for |form|.
std::string GetDisplayOriginForSettings(const autofill::PasswordForm& form) {
  bool is_android_uri = false;
  bool is_clickable = false;
  GURL link_url;  // TODO(crbug.com/617094) Also display link_url.
  std::string human_readable_origin =
      password_manager::GetShownOriginAndLinkUrl(form, &is_android_uri,
                                                 &link_url,
                                                 &is_clickable);
  if (!is_clickable) {
    DCHECK(is_android_uri);
    human_readable_origin = password_manager::StripAndroidAndReverse(
        human_readable_origin);
    human_readable_origin = human_readable_origin +
        l10n_util::GetStringUTF8(IDS_PASSWORDS_ANDROID_URI_SUFFIX);
  }
  return human_readable_origin;
}

}  // namespace

PasswordUIViewAndroid::PasswordUIViewAndroid(JNIEnv* env, jobject obj)
    : password_manager_presenter_(this), weak_java_ui_controller_(env, obj) {}

PasswordUIViewAndroid::~PasswordUIViewAndroid() {}

void PasswordUIViewAndroid::Destroy(JNIEnv*, const JavaParamRef<jobject>&) {
  delete this;
}

Profile* PasswordUIViewAndroid::GetProfile() {
  return ProfileManager::GetLastUsedProfile();
}

void PasswordUIViewAndroid::ShowPassword(
    size_t index,
    const std::string& origin_url,
    const std::string& username,
    const base::string16& password_value) {
  NOTIMPLEMENTED();
}

void PasswordUIViewAndroid::SetPasswordList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& password_list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordListAvailable(
        env, ui_controller, static_cast<int>(password_list.size()));
  }
}

void PasswordUIViewAndroid::SetPasswordExceptionList(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordExceptionListAvailable(
        env, ui_controller, static_cast<int>(password_exception_list.size()));
  }
}

void PasswordUIViewAndroid::UpdatePasswordLists(JNIEnv* env,
                                                const JavaParamRef<jobject>&) {
  password_manager_presenter_.UpdatePasswordLists();
}

ScopedJavaLocalRef<jobject> PasswordUIViewAndroid::GetSavedPasswordEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    int index) {
  const autofill::PasswordForm* form =
      password_manager_presenter_.GetPassword(index);
  if (!form) {
    return Java_PasswordUIView_createSavedPasswordEntry(
        env, ConvertUTF8ToJavaString(env, std::string()),
        ConvertUTF16ToJavaString(env, base::string16()));
  }
  std::string human_readable_origin = GetDisplayOriginForSettings(*form);
  return Java_PasswordUIView_createSavedPasswordEntry(
      env, ConvertUTF8ToJavaString(env, human_readable_origin),
      ConvertUTF16ToJavaString(env, form->username_value));
}

ScopedJavaLocalRef<jstring> PasswordUIViewAndroid::GetSavedPasswordException(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    int index) {
  const autofill::PasswordForm* form =
      password_manager_presenter_.GetPasswordException(index);
  if (!form)
    return ConvertUTF8ToJavaString(env, std::string());
  std::string human_readable_origin = GetDisplayOriginForSettings(*form);
  return ConvertUTF8ToJavaString(env, human_readable_origin);
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordEntry(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    int index) {
  password_manager_presenter_.RemoveSavedPassword(index);
}

void PasswordUIViewAndroid::HandleRemoveSavedPasswordException(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    int index) {
  password_manager_presenter_.RemovePasswordException(index);
}

ScopedJavaLocalRef<jstring> GetAccountDashboardURL(
    JNIEnv* env,
    const JavaParamRef<jclass>&) {
  return ConvertUTF8ToJavaString(
      env, password_manager::kPasswordManagerAccountDashboardURL);
}

// static
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PasswordUIViewAndroid* controller = new PasswordUIViewAndroid(env, obj);
  return reinterpret_cast<intptr_t>(controller);
}

bool PasswordUIViewAndroid::RegisterPasswordUIViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
