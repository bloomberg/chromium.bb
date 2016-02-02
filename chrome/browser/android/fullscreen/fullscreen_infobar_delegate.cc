// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/fullscreen/fullscreen_infobar_delegate.h"

#include "base/android/jni_string.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "grit/components_strings.h"
#include "jni/FullscreenInfoBarDelegate_jni.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
jlong LaunchFullscreenInfoBar(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              const JavaParamRef<jobject>& tab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, tab);
  GURL origin = tab_android->GetURL().GetOrigin();
  FullscreenInfoBarDelegate* delegate = new FullscreenInfoBarDelegate(
      env, obj, origin);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab_android->web_contents());
  infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(make_scoped_ptr(delegate)));
  return reinterpret_cast<intptr_t>(delegate);
}

bool FullscreenInfoBarDelegate::RegisterFullscreenInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

FullscreenInfoBarDelegate::FullscreenInfoBarDelegate(
    JNIEnv* env, jobject obj, GURL origin)
    : origin_(origin) {
  j_delegate_.Reset(env, obj);
}

FullscreenInfoBarDelegate::~FullscreenInfoBarDelegate() {
  if (!j_delegate_.is_null()) {
    Java_FullscreenInfoBarDelegate_onInfoBarDismissed(
        base::android::AttachCurrentThread(), j_delegate_.obj());
  }
}

void FullscreenInfoBarDelegate::CloseFullscreenInfoBar(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  j_delegate_.Reset();
  if (infobar() && infobar()->owner())
    infobar()->owner()->RemoveInfoBar(infobar());
}

infobars::InfoBarDelegate::InfoBarIdentifier
FullscreenInfoBarDelegate::GetIdentifier() const {
  return FULLSCREEN_INFOBAR_DELEGATE;
}

int FullscreenInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_FULLSCREEN;
}

base::string16 FullscreenInfoBarDelegate::GetMessageText() const {
  Profile* profile =
      ProfileManager::GetActiveUserProfile()->GetOriginalProfile();
  std::string language =
      profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  return l10n_util::GetStringFUTF16(
      IDS_FULLSCREEN_INFOBAR_TEXT,
      url_formatter::FormatUrlForSecurityDisplay(origin_, language));
}

base::string16 FullscreenInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_FULLSCREEN_INFOBAR_ALLOW_BUTTON :
      IDS_FULLSCREEN_INFOBAR_EXIT_FULLSCREEN_BUTTON);
}

bool FullscreenInfoBarDelegate::Accept() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_origin =
      base::android::ConvertUTF8ToJavaString(env, origin_.spec());
  Java_FullscreenInfoBarDelegate_onFullscreenAllowed(
      env, j_delegate_.obj(), j_origin.obj());
  return true;
}

bool FullscreenInfoBarDelegate::Cancel() {
  Java_FullscreenInfoBarDelegate_onFullscreenCancelled(
      base::android::AttachCurrentThread(), j_delegate_.obj());
  return true;
}
