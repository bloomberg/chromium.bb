// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_WEBSITE_SETTINGS_POPUP_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_WEBSITE_SETTINGS_POPUP_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

namespace content {
class WebContents;
}

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser
enum PageInfoConnectionType {
  CONNECTION_UNKNOWN,
  CONNECTION_ENCRYPTED,
  CONNECTION_MIXED_CONTENT,
  CONNECTION_UNENCRYPTED,
  CONNECTION_ENCRYPTED_ERROR,
  CONNECTION_INTERNAL_PAGE,
};

// Android implementation of the website settings UI.
class WebsiteSettingsPopupAndroid : public WebsiteSettingsUI {
 public:
  WebsiteSettingsPopupAndroid(JNIEnv* env,
                              jobject java_website_settings,
                              content::WebContents* web_contents);
  ~WebsiteSettingsPopupAndroid() override;
  void Destroy(JNIEnv* env, jobject obj);
  void OnPermissionSettingChanged(JNIEnv* env,
                                  jobject obj,
                                  jint type,
                                  jint setting);

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;
  void SetSelectedTab(WebsiteSettingsUI::TabId tab_id) override;

  static bool RegisterWebsiteSettingsPopupAndroid(JNIEnv* env);

 private:
  // The presenter that controlls the Website Settings UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_WEBSITE_SETTINGS_POPUP_ANDROID_H_
