// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_DATA_FETCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_DATA_FETCHER_ANDROID_H_

#include "base/android/jni_android.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"

namespace banners {

// Fetches data required to show a banner for the URL currently shown by the
// WebContents.  Extends the regular fetch to add support for Android apps.
class AppBannerDataFetcherAndroid : public AppBannerDataFetcher {
 public:
  AppBannerDataFetcherAndroid(
      content::WebContents* web_contents,
      base::WeakPtr<Delegate> weak_delegate,
      int ideal_icon_size);

  // Saves information about the Android app being promoted by the current page,
  // then continues the creation pipeline.
  bool ContinueFetching(const base::string16& app_title,
                        const std::string& app_package,
                        base::android::ScopedJavaLocalRef<jobject> app_data,
                        const GURL& image_url);

 protected:
  ~AppBannerDataFetcherAndroid() override;

  std::string GetBannerType() override;
  std::string GetAppIdentifier() override;
  infobars::InfoBar* CreateBanner(const SkBitmap* icon,
                                  const base::string16& title) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> native_app_data_;
  std::string native_app_package_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerDataFetcherAndroid);
};

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_DATA_FETCHER_ANDROID_H_
