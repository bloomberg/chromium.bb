// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PAGE_INFO_WEBSITE_SETTINGS_POPUP_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PAGE_INFO_WEBSITE_SETTINGS_POPUP_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"

namespace content {
class WebContents;
}

class SearchGeolocationService;

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
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void RecordWebsiteSettingsAction(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint action);

  // WebsiteSettingsUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

  static bool RegisterWebsiteSettingsPopupAndroid(JNIEnv* env);

 private:
  // The presenter that controlls the Website Settings UI.
  std::unique_ptr<WebsiteSettings> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;

  // Owned by the profile.
  SearchGeolocationService* search_geolocation_service_;

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PAGE_INFO_WEBSITE_SETTINGS_POPUP_ANDROID_H_
