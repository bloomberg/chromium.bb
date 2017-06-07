// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate_android.h"
#include "jni/GroupedPermissionInfoBar_jni.h"

GroupedPermissionInfoBar::GroupedPermissionInfoBar(
    std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

GroupedPermissionInfoBar::~GroupedPermissionInfoBar() {
}

void GroupedPermissionInfoBar::ProcessButton(int action) {
  // Check if the delegate asked us to display a persistence toggle. If so,
  // inform it of the toggle state.
  GroupedPermissionInfoBarDelegate* delegate = GetDelegate();
  if (delegate->ShouldShowPersistenceToggle()) {
    delegate->set_persist(Java_GroupedPermissionInfoBar_isPersistSwitchOn(
        base::android::AttachCurrentThread(), GetJavaInfoBar()));
  }

  ConfirmInfoBar::ProcessButton(action);
}

base::android::ScopedJavaLocalRef<jobject>
GroupedPermissionInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  GroupedPermissionInfoBarDelegate* delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());
  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  base::android::ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));

  std::vector<base::string16> permission_strings;
  std::vector<int> permission_icons;
  std::vector<int> content_settings_types;

  for (size_t i = 0; i < delegate->PermissionCount(); i++) {
    permission_strings.push_back(delegate->GetMessageTextFragment(i));
    permission_icons.push_back(
        ResourceMapper::MapFromChromiumId(delegate->GetIconIdForPermission(i)));
    content_settings_types.push_back(delegate->GetContentSettingType(i));
  }

  return Java_GroupedPermissionInfoBar_create(
      env, GetTab()->GetJavaObject(),
      base::android::ToJavaIntArray(env, content_settings_types), message_text,
      link_text, ok_button_text, cancel_button_text,
      delegate->ShouldShowPersistenceToggle(),
      base::android::ToJavaArrayOfStrings(env, permission_strings),
      base::android::ToJavaIntArray(env, permission_icons));
}

GroupedPermissionInfoBarDelegate* GroupedPermissionInfoBar::GetDelegate() {
  return static_cast<GroupedPermissionInfoBarDelegate*>(delegate());
}
