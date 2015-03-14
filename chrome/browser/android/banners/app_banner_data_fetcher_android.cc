// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_data_fetcher_android.h"

#include "chrome/browser/android/banners/app_banner_infobar_delegate.h"
#include "chrome/browser/ui/android/infobars/app_banner_infobar.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace banners {

AppBannerDataFetcherAndroid::AppBannerDataFetcherAndroid(
    content::WebContents* web_contents,
    base::WeakPtr<Delegate> weak_delegate,
    int ideal_icon_size)
    : AppBannerDataFetcher(web_contents, weak_delegate, ideal_icon_size) {
}

AppBannerDataFetcherAndroid::~AppBannerDataFetcherAndroid() {
}

std::string AppBannerDataFetcherAndroid::GetBannerType() {
  return native_app_data_.is_null()
      ? AppBannerDataFetcher::GetBannerType() : "android";
}

bool AppBannerDataFetcherAndroid::ContinueFetching(
    const base::string16& app_title,
    const std::string& app_package,
    base::android::ScopedJavaLocalRef<jobject> app_data,
    const GURL& image_url) {
  set_app_title(app_title);
  native_app_package_ = app_package;
  native_app_data_.Reset(app_data);
  return FetchIcon(image_url);
}

std::string AppBannerDataFetcherAndroid::GetAppIdentifier() {
  return native_app_data_.is_null()
      ? AppBannerDataFetcher::GetAppIdentifier() : native_app_package_;
}

infobars::InfoBar* AppBannerDataFetcherAndroid::CreateBanner(
    const SkBitmap* icon,
    const base::string16& title) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  infobars::InfoBar* infobar = nullptr;
  if (native_app_data_.is_null()) {
    scoped_ptr<AppBannerInfoBarDelegate> delegate(
        new AppBannerInfoBarDelegate(title,
                                     new SkBitmap(*icon),
                                     web_app_data()));

    infobar = new AppBannerInfoBar(delegate.Pass(), web_app_data().start_url);
    if (infobar)
      RecordDidShowBanner("AppBanner.WebApp.Shown");
  } else {
    scoped_ptr<AppBannerInfoBarDelegate> delegate(
        new AppBannerInfoBarDelegate(title,
                                     new SkBitmap(*icon),
                                     native_app_data_,
                                     native_app_package_));
    infobar = new AppBannerInfoBar(delegate.Pass(), native_app_data_);
    if (infobar)
      RecordDidShowBanner("AppBanner.NativeApp.Shown");
  }

  return infobar;
}

}  // namespace banners
