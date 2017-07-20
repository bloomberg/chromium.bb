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

using base::android::JavaRef;
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
  base::string16 ok_button_text =
      GetTextFor(PermissionInfoBarDelegate::BUTTON_OK);
  base::string16 cancel_button_text =
      GetTextFor(PermissionInfoBarDelegate::BUTTON_CANCEL);
  PermissionInfoBarDelegate* delegate = GetDelegate();
  base::string16 message_text = delegate->GetMessageText();
  base::string16 link_text = delegate->GetLinkText();

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  std::vector<int> content_settings{delegate->content_settings_types()};
  return CreateRenderInfoBarHelper(
      env, GetEnumeratedIconId(), GetTab()->GetJavaObject(), java_bitmap,
      message_text, link_text, ok_button_text, cancel_button_text,
      content_settings, delegate->ShouldShowPersistenceToggle());
}

void PermissionInfoBar::ProcessButton(int action) {
  // Check if the delegate asked us to display a persistence toggle. If so,
  // inform it of the toggle state.
  PermissionInfoBarDelegate* delegate = GetDelegate();
  if (delegate->ShouldShowPersistenceToggle()) {
    delegate->set_persist(
        IsSwitchOn(base::android::AttachCurrentThread(), GetJavaInfoBar()));
  }

  ConfirmInfoBar::ProcessButton(action);
}

ScopedJavaLocalRef<jobject> PermissionInfoBar::CreateRenderInfoBarHelper(
    JNIEnv* env,
    int enumerated_icon_id,
    const JavaRef<jobject>& tab,
    const ScopedJavaLocalRef<jobject>& icon_bitmap,
    const base::string16& message_text,
    const base::string16& link_text,
    const base::string16& ok_button_text,
    const base::string16& cancel_button_text,
    std::vector<int>& content_settings,
    bool show_persistence_toggle) {
  ScopedJavaLocalRef<jstring> message_text_java =
      base::android::ConvertUTF16ToJavaString(env, message_text);
  ScopedJavaLocalRef<jstring> link_text_java =
      base::android::ConvertUTF16ToJavaString(env, link_text);
  ScopedJavaLocalRef<jstring> ok_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, ok_button_text);
  ScopedJavaLocalRef<jstring> cancel_button_text_java =
      base::android::ConvertUTF16ToJavaString(env, cancel_button_text);

  ScopedJavaLocalRef<jintArray> content_settings_types =
      base::android::ToJavaIntArray(env, content_settings);
  return Java_PermissionInfoBar_create(
      env, tab, enumerated_icon_id, icon_bitmap, message_text_java,
      link_text_java, ok_button_text_java, cancel_button_text_java,
      content_settings_types, show_persistence_toggle);
}

bool PermissionInfoBar::IsSwitchOn(JNIEnv* env, jobject info_bar_obj) {
  return Java_PermissionInfoBar_isPersistSwitchOn(env, info_bar_obj);
}
