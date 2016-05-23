// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/media_stream_infobar_delegate_android.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/ConfirmInfoBar_jni.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// InfoBarService -------------------------------------------------------------

std::unique_ptr<infobars::InfoBar> InfoBarService::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return base::WrapUnique(new ConfirmInfoBar(std::move(delegate)));
}


// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

ConfirmInfoBar::~ConfirmInfoBar() {
}

base::android::ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
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
  }

  return Java_ConfirmInfoBar_create(
      env, GetWindowAndroid().obj(), GetEnumeratedIconId(), java_bitmap.obj(),
      message_text.obj(), link_text.obj(), ok_button_text.obj(),
      cancel_button_text.obj(),
      base::android::ToJavaIntArray(env, content_settings).obj());
}

base::android::ScopedJavaLocalRef<jobject> ConfirmInfoBar::GetWindowAndroid() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  DCHECK(web_contents);
  content::ContentViewCore* cvc =
      content::ContentViewCore::FromWebContents(web_contents);
  DCHECK(cvc);
  return cvc->GetWindowAndroid()->GetJavaObject();
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
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

bool RegisterConfirmInfoBar(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
