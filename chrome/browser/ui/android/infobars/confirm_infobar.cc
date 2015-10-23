// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/ConfirmInfoBarDelegate_jni.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/media_stream_infobar_delegate_android.h"
#endif

// InfoBarService -------------------------------------------------------------

scoped_ptr<infobars::InfoBar> InfoBarService::CreateConfirmInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  return make_scoped_ptr(new ConfirmInfoBar(delegate.Pass()));
}


// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarAndroid(delegate.Pass()), java_confirm_delegate_() {
}

ConfirmInfoBar::~ConfirmInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  java_confirm_delegate_.Reset(Java_ConfirmInfoBarDelegate_create(env));
  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  base::android::ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetLinkText());

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  std::vector<int> content_settings;
  if (delegate->AsPermissionInfobarDelegate()) {
    content_settings.push_back(
        delegate->AsPermissionInfobarDelegate()->content_setting());
#if defined(OS_ANDROID)
  } else if (delegate->AsMediaStreamInfoBarDelegateAndroid()) {
    MediaStreamInfoBarDelegateAndroid* media_delegate =
        delegate->AsMediaStreamInfoBarDelegateAndroid();
    if (media_delegate->IsRequestingVideoAccess()) {
      content_settings.push_back(
          ContentSettingsType::CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
    }
    if (media_delegate->IsRequestingMicrophoneAccess()) {
      content_settings.push_back(
          ContentSettingsType::CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
    }
#endif
  }

  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  DCHECK(web_contents);
  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents);
  DCHECK(cvc);
  base::android::ScopedJavaLocalRef<jobject> jwindow_android =
      cvc->GetWindowAndroid()->GetJavaObject();

  return Java_ConfirmInfoBarDelegate_showConfirmInfoBar(
      env, java_confirm_delegate_.obj(),
      jwindow_android.obj(), GetEnumeratedIconId(), java_bitmap.obj(),
      message_text.obj(), link_text.obj(), ok_button_text.obj(),
      cancel_button_text.obj(),
      base::android::ToJavaIntArray(env, content_settings).obj());
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env, jobject obj) {
  if (!owner())
      return; // We're closing; don't call anything, it might access the owner.

  if (GetDelegate()->LinkClicked(NEW_FOREGROUND_TAB))
    RemoveSelf();
}

void ConfirmInfoBar::ProcessButton(int action) {
  if (!owner())
    return; // We're closing; don't call anything, it might access the owner.

  DCHECK((action == InfoBarAndroid::ACTION_OK) ||
      (action == InfoBarAndroid::ACTION_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if ((action == InfoBarAndroid::ACTION_OK) ?
      delegate->Accept() : delegate->Cancel())
    RemoveSelf();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

base::string16 ConfirmInfoBar::GetTextFor(
    ConfirmInfoBarDelegate::InfoBarButton button) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  return (delegate->GetButtons() & button) ?
      delegate->GetButtonLabel(button) : base::string16();
}


// Native JNI methods ---------------------------------------------------------

bool RegisterConfirmInfoBarDelegate(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
