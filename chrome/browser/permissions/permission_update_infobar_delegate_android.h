// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_

#include <jni.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace content {
class WebContents;
}

namespace ui {
class WindowAndroid;
}

// An infobar delegate to be used for requesting missing Android runtime
// permissions for previously allowed ContentSettingsTypes.
class PermissionUpdateInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using PermissionUpdatedCallback = base::Callback<void(bool)>;

  // Creates an infobar to resolve conflicts in Android runtime permissions
  // and adds the infobar to |infobar_service|.  Returns the infobar if it was
  // successfully added.
  static infobars::InfoBar* Create(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      const PermissionUpdatedCallback& callback);

  // Return whether the runtime permissions currently granted to Chrome by
  // Android are compatible with ContentSettingTypes previously granted to a
  // site by the user.
  static bool ShouldShowPermissionInfobar(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types);

  static bool RegisterPermissionUpdateInfoBarDelegate(JNIEnv* env);

  void OnPermissionResult(
      JNIEnv* env, jobject obj, jboolean all_permissions_granted);

 private:
  PermissionUpdateInfoBarDelegate(
      content::WebContents* web_contents,
      const std::vector<ContentSettingsType>& content_settings_types,
      const PermissionUpdatedCallback& callback);
  ~PermissionUpdateInfoBarDelegate() override;

  // PermissionInfoBarDelegate:
  int GetIconId() const override;
  base::string16 GetMessageText() const override;

  // ConfirmInfoBarDelegate:
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // InfoBarDelegate:
  void InfoBarDismissed() override;

  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;
  std::vector<ContentSettingsType> content_settings_types_;
  PermissionUpdatedCallback callback_;
  ui::WindowAndroid* window_android_;

  DISALLOW_COPY_AND_ASSIGN(PermissionUpdateInfoBarDelegate);
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_INFOBAR_DELEGATE_ANDROID_H_
