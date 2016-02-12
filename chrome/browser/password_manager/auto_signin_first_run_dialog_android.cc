// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/auto_signin_first_run_dialog_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "jni/AutoSigninFirstRunDialog_jni.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/range/range.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;

AutoSigninFirstRunDialogAndroid::AutoSigninFirstRunDialogAndroid(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

AutoSigninFirstRunDialogAndroid::~AutoSigninFirstRunDialogAndroid() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      profile->GetPrefs());
}

void AutoSigninFirstRunDialogAndroid::ShowDialog() {
  JNIEnv* env = AttachCurrentThread();
  bool is_smartlock_branding_enabled =
      password_bubble_experiment::IsSmartLockBrandingEnabled(
          ProfileSyncServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents_->GetBrowserContext())));
  base::string16 explanation;
  gfx::Range explanation_link_range = gfx::Range();
  GetBrandedTextAndLinkRange(
      is_smartlock_branding_enabled,
      IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_SMART_LOCK_WELCOME,
      IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_DEFAULT_WELCOME, &explanation,
      &explanation_link_range);
  gfx::NativeWindow native_window = web_contents_->GetTopLevelNativeWindow();
  base::android::ScopedJavaGlobalRef<jobject> java_dialog_global;
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_FIRST_RUN_TITLE);
  base::string16 ok_button_text =
      l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_OK);
  base::string16 turn_off_button_text =
      l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF);

  Java_AutoSigninFirstRunDialog_createDialog(
      env, native_window->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this),
      base::android::ConvertUTF16ToJavaString(env, message).obj(),
      base::android::ConvertUTF16ToJavaString(env, explanation).obj(),
      explanation_link_range.start(), explanation_link_range.end(),
      base::android::ConvertUTF16ToJavaString(env, ok_button_text).obj(),
      base::android::ConvertUTF16ToJavaString(env, turn_off_button_text).obj());
}

void AutoSigninFirstRunDialogAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AutoSigninFirstRunDialogAndroid::OnOkClicked(JNIEnv* env, jobject obj) {}

void AutoSigninFirstRunDialogAndroid::OnTurnOffClicked(JNIEnv* env,
                                                       jobject obj) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  password_bubble_experiment::TurnOffAutoSignin(profile->GetPrefs());
}

void AutoSigninFirstRunDialogAndroid::CancelDialog(JNIEnv* env, jobject obj) {}

void AutoSigninFirstRunDialogAndroid::OnLinkClicked(JNIEnv* env, jobject obj) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(password_manager::kPasswordManagerAccountDashboardURL),
      content::Referrer(), NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_initiated */));
}

bool RegisterAutoSigninFirstRunDialogAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
