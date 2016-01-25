// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_

#include "chrome/browser/banners/app_banner_manager.h"

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

namespace banners {

class AppBannerManagerDesktop
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerDesktop> {

 public:
  static bool IsEnabled();

 protected:
  AppBannerDataFetcher* CreateAppBannerDataFetcher(
      base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
      bool is_debug_mode) override;

 private:
  explicit AppBannerManagerDesktop(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AppBannerManagerDesktop>;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerDesktop);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_DESKTOP_H_
