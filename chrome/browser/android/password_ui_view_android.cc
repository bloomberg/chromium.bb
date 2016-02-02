// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/password_ui_view_android.h"

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/common/experiments.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/prefs/pref_service.h"
#include "jni/PasswordUIView_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

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
    const std::vector<scoped_ptr<autofill::PasswordForm>>& password_list,
    bool show_passwords) {
  // Android just ignores the |show_passwords| argument.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordListAvailable(
        env, ui_controller.obj(), static_cast<int>(password_list.size()));
  }
}

void PasswordUIViewAndroid::SetPasswordExceptionList(
    const std::vector<scoped_ptr<autofill::PasswordForm>>&
        password_exception_list) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> ui_controller = weak_java_ui_controller_.get(env);
  if (!ui_controller.is_null()) {
    Java_PasswordUIView_passwordExceptionListAvailable(
        env,
        ui_controller.obj(),
        static_cast<int>(password_exception_list.size()));
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
        env,
        ConvertUTF8ToJavaString(env, std::string()).obj(),
        ConvertUTF16ToJavaString(env, base::string16()).obj());
  }
  std::string human_readable_origin = password_manager::GetHumanReadableOrigin(
      *form, GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
  return Java_PasswordUIView_createSavedPasswordEntry(
      env, ConvertUTF8ToJavaString(env, human_readable_origin).obj(),
      ConvertUTF16ToJavaString(env, form->username_value).obj());
}

ScopedJavaLocalRef<jstring> PasswordUIViewAndroid::GetSavedPasswordException(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    int index) {
  const autofill::PasswordForm* form =
      password_manager_presenter_.GetPasswordException(index);
  if (!form)
    return ConvertUTF8ToJavaString(env, std::string());
  std::string human_readable_origin = password_manager::GetHumanReadableOrigin(
      *form, GetProfile()->GetPrefs()->GetString(prefs::kAcceptLanguages));
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

static jboolean ShouldUseSmartLockBranding(JNIEnv* env,
                                           const JavaParamRef<jclass>&) {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(
          ProfileManager::GetLastUsedProfile());
  return password_bubble_experiment::IsSmartLockBrandingEnabled(sync_service);
}

// static
static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  PasswordUIViewAndroid* controller = new PasswordUIViewAndroid(env, obj);
  return reinterpret_cast<intptr_t>(controller);
}

bool PasswordUIViewAndroid::RegisterPasswordUIViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
