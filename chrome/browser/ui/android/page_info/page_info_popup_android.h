// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PAGE_INFO_PAGE_INFO_POPUP_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PAGE_INFO_PAGE_INFO_POPUP_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"

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

// Android implementation of the page info UI.
class PageInfoPopupAndroid : public PageInfoUI {
 public:
  PageInfoPopupAndroid(JNIEnv* env,
                       jobject java_page_info,
                       content::WebContents* web_contents);
  ~PageInfoPopupAndroid() override;
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void RecordPageInfoAction(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            jint action);

  // PageInfoUI implementations.
  void SetCookieInfo(const CookieInfoList& cookie_info_list) override;
  void SetPermissionInfo(const PermissionInfoList& permission_info_list,
                         ChosenObjectInfoList chosen_object_info_list) override;
  void SetIdentityInfo(const IdentityInfo& identity_info) override;

 private:
  // Returns an optional value which is set if this permission should be
  // displayed in Page Info. Most permissions will only be displayed if they are
  // set to some non-default value, but there are some permissions which require
  // customized behavior.
  base::Optional<ContentSetting> GetSettingToDisplay(
      const PermissionInfo& permission);

  // The presenter that controlls the Page Info UI.
  std::unique_ptr<PageInfo> presenter_;

  // The java prompt implementation.
  base::android::ScopedJavaGlobalRef<jobject> popup_jobject_;

  // Owned by the profile.
  SearchGeolocationService* search_geolocation_service_;

  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoPopupAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PAGE_INFO_PAGE_INFO_POPUP_ANDROID_H_
