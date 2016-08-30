// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "url/gurl.h"

namespace banners {
class AppBannerInfoBarDelegateAndroid;
}  // namespace banners


class AppBannerInfoBarAndroid : public ConfirmInfoBar {
 public:
  // Constructs an AppBannerInfoBarAndroid promoting a native app.
  AppBannerInfoBarAndroid(
      std::unique_ptr<banners::AppBannerInfoBarDelegateAndroid> delegate,
      const base::android::ScopedJavaGlobalRef<jobject>& japp_data);

  // Constructs an AppBannerInfoBarAndroid promoting a web app.
  AppBannerInfoBarAndroid(
      std::unique_ptr<banners::AppBannerInfoBarDelegateAndroid> delegate,
      const GURL& app_url,
      bool is_webapk);

  ~AppBannerInfoBarAndroid() override;

  // Called when the installation state of the app may have changed.
  // Updates the InfoBar visuals to match the new state and re-enables controls
  // that may have been disabled.
  void OnInstallStateChanged(int new_state);

 private:
  // InfoBarAndroid overrides.
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  // Native app: Details about the app.
  base::android::ScopedJavaGlobalRef<jobject> japp_data_;

  // Web app: URL for the app.
  GURL app_url_;

  // Indicates whether the info bar is for installing a WebAPK.
  bool is_webapk_;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBarAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_ANDROID_H_
