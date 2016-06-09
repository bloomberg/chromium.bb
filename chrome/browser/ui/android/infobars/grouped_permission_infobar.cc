// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/grouped_permission_infobar.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/permissions/grouped_permission_infobar_delegate.h"
#include "jni/GroupedPermissionInfoBar_jni.h"

GroupedPermissionInfoBar::GroupedPermissionInfoBar(
    std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

GroupedPermissionInfoBar::~GroupedPermissionInfoBar() {
}

bool GroupedPermissionInfoBar::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void GroupedPermissionInfoBar::SetPermissionState(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jbooleanArray>& permissions) {

  for (int i = 0; i < GetDelegate()->GetPermissionCount(); i++) {
      jboolean value;
      env->GetBooleanArrayRegion(permissions.obj(), i, 1, &value);
      GetDelegate()->ToggleAccept(i, value);
  }
}

base::android::ScopedJavaLocalRef<jobject>
GroupedPermissionInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  GroupedPermissionInfoBarDelegate* delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  base::android::ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));

  std::vector<base::string16> permission_strings;
  std::vector<int> permission_icons;
  std::vector<int> content_settings_types;

  for (int i = 0; i < delegate->GetPermissionCount(); i++) {
    permission_strings.push_back(delegate->GetMessageTextFragment(i));
    permission_icons.push_back(
        ResourceMapper::MapFromChromiumId(delegate->GetIconIdForPermission(i)));
    content_settings_types.push_back(delegate->GetContentSettingType(i));
  }

  return Java_GroupedPermissionInfoBar_create(
      env, message_text.obj(), ok_button_text.obj(), cancel_button_text.obj(),
      base::android::ToJavaIntArray(env, permission_icons).obj(),
      base::android::ToJavaArrayOfStrings(env, permission_strings).obj(),
      GetWindowAndroid().obj(),
      base::android::ToJavaIntArray(env, content_settings_types).obj());
}

void GroupedPermissionInfoBar::SetJavaInfoBar(
    const base::android::JavaRef<jobject>& java_info_bar) {
  InfoBarAndroid::SetJavaInfoBar(java_info_bar);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_GroupedPermissionInfoBar_setNativePtr(env, java_info_bar.obj(),
                                             reinterpret_cast<intptr_t>(this));
}

GroupedPermissionInfoBarDelegate* GroupedPermissionInfoBar::GetDelegate() {
  return static_cast<GroupedPermissionInfoBarDelegate*>(delegate());
}

std::unique_ptr<infobars::InfoBar>
GroupedPermissionInfoBarDelegate::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate) {
  return std::unique_ptr<infobars::InfoBar>(
      new GroupedPermissionInfoBar(std::move(delegate)));
}
