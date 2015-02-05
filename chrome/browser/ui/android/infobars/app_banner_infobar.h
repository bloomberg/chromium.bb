// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "chrome/browser/ui/android/infobars/confirm_infobar.h"
#include "url/gurl.h"

namespace banners {
class AppBannerInfoBarDelegate;
}  // namespace banners


class AppBannerInfoBar : public ConfirmInfoBar {
 public:
  // Constructs an AppBannerInfoBar promoting a web app.
  AppBannerInfoBar(
      scoped_ptr<banners::AppBannerInfoBarDelegate> delegate,
      const GURL& app_url);

  ~AppBannerInfoBar() override;

 private:
  // InfoBarAndroid overrides.
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env) override;

  // Web app: URL for the app.
  GURL app_url_;

  base::android::ScopedJavaGlobalRef<jobject> java_infobar_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_APP_BANNER_INFOBAR_H_
