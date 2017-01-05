// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CONNECTION_INFO_POPUP_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CONNECTION_INFO_POPUP_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

namespace content {
class WebContents;
}

// Android implementation of the website settings UI which displays detailed
// connection and certificate information for the website.
class ConnectionInfoPopupAndroid : public WebsiteSettingsUI {
 public:
  ConnectionInfoPopupAndroid(JNIEnv* env,
                             jobject java_website_settings,
                             content::WebContents* web_contents);
  ~ConnectionInfoPopupAndroid() override;
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Revokes any current user exceptions for bypassing SSL error interstitials
  // on this page.
  void ResetCertDecisions(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& java_web_contents);

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

  static bool RegisterConnectionInfoPopupAndroid(JNIEnv* env);

 private:
  // The presenter that controls the Website Settings UI.
  std::unique_ptr<WebsiteSettings> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionInfoPopupAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PAGE_INFO_CONNECTION_INFO_POPUP_ANDROID_H_
