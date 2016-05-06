// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_EMULATION_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_EMULATION_H_

#include "chrome/browser/banners/app_banner_manager.h"

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

namespace banners {

class AppBannerManagerEmulation
    : public AppBannerManager,
      public content::WebContentsUserData<AppBannerManagerEmulation> {

 protected:
  AppBannerDataFetcher* CreateAppBannerDataFetcher(
      base::WeakPtr<AppBannerDataFetcher::Delegate> weak_delegate,
      bool is_debug_mode) override;

 private:
  explicit AppBannerManagerEmulation(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AppBannerManagerEmulation>;

  DISALLOW_COPY_AND_ASSIGN(AppBannerManagerEmulation);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_MANAGER_EMULATION_H_
