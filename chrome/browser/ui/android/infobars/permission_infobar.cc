// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/permission_infobar.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "jni/PermissionInfoBar_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::ScopedJavaLocalRef;

std::unique_ptr<infobars::InfoBar> CreatePermissionInfoBar(
    std::unique_ptr<PermissionInfoBarDelegate> delegate) {
  return base::WrapUnique(new PermissionInfoBar(std::move(delegate)));
}

PermissionInfoBar::PermissionInfoBar(
    std::unique_ptr<PermissionInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

PermissionInfoBar::~PermissionInfoBar() = default;

PermissionInfoBarDelegate* PermissionInfoBar::GetDelegate() {
  return delegate()->AsPermissionInfoBarDelegate();
}

ScopedJavaLocalRef<jobject> PermissionInfoBar::CreateRenderInfoBar(
    JNIEnv* env) {
  ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(PermissionInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(PermissionInfoBarDelegate::BUTTON_CANCEL));
  PermissionInfoBarDelegate* delegate = GetDelegate();
  ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  std::vector<int> content_settings{delegate->content_settings_types()};

  return Java_PermissionInfoBar_create(
      env, GetTab()->GetJavaObject(), GetEnumeratedIconId(), java_bitmap.obj(),
      message_text.obj(), link_text.obj(), ok_button_text.obj(),
      cancel_button_text.obj(),
      base::android::ToJavaIntArray(env, content_settings).obj(),
      delegate->ShouldShowPersistenceToggle());
}

void PermissionInfoBar::ProcessButton(int action) {
  // Check if the delegate asked us to display a persistence toggle. If so,
  // inform it of the toggle state.
  PermissionInfoBarDelegate* delegate = GetDelegate();
  if (delegate->ShouldShowPersistenceToggle()) {
    delegate->set_persist(Java_PermissionInfoBar_isPersistSwitchOn(
        base::android::AttachCurrentThread(), GetJavaInfoBar()));
  }

  ConfirmInfoBar::ProcessButton(action);
}
