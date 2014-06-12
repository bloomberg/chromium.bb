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

// Android implementation of the website settings UI.
class WebsiteSettingsPopupAndroid : public WebsiteSettingsUI {
 public:
  WebsiteSettingsPopupAndroid(JNIEnv* env,
                              jobject java_website_settings,
                              content::WebContents* web_contents);
  virtual ~WebsiteSettingsPopupAndroid();
  void Destroy(JNIEnv* env, jobject obj);

  // WebsiteSettingsUI implementations.
  virtual void SetCookieInfo(const CookieInfoList& cookie_info_list) OVERRIDE;
  virtual void SetPermissionInfo(
      const PermissionInfoList& permission_info_list) OVERRIDE;
  virtual void SetIdentityInfo(const IdentityInfo& identity_info) OVERRIDE;
  virtual void SetFirstVisit(const base::string16& first_visit) OVERRIDE;
  virtual void SetSelectedTab(WebsiteSettingsUI::TabId tab_id) OVERRIDE;

  static bool RegisterWebsiteSettingsPopupAndroid(JNIEnv* env);

 private:
  // The presenter that controlls the Website Settings UI.
  scoped_ptr<WebsiteSettings> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsPopupAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_WEBSITE_SETTINGS_POPUP_ANDROID_H_
