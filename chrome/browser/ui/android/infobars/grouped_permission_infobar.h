// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

class GroupedPermissionInfoBarDelegate;

class GroupedPermissionInfoBar : public ConfirmInfoBar {
 public:
  explicit GroupedPermissionInfoBar(
      std::unique_ptr<GroupedPermissionInfoBarDelegate> delegate);
  ~GroupedPermissionInfoBar() override;

  void SetPermissionState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbooleanArray>& permissions);

  static bool Register(JNIEnv* env);

 private:
  void ProcessButton(int action) override;

  // InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;
  void SetJavaInfoBar(
      const base::android::JavaRef<jobject>& java_info_bar) override;

  GroupedPermissionInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(GroupedPermissionInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_GROUPED_PERMISSION_INFOBAR_H_
