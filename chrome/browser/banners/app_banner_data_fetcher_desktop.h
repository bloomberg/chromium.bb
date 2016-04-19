// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_DESKTOP_H_
#define CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_DESKTOP_H_

#include "base/macros.h"
#include "chrome/browser/banners/app_banner_data_fetcher.h"

namespace extensions {
class BookmarkAppHelper;
class Extension;
}  // namespace extensions

namespace banners {

// Fetches data required to show a banner for the URL currently shown by the
// WebContents. Extends the regular fetch to support desktop web apps.
class AppBannerDataFetcherDesktop : public AppBannerDataFetcher {
 public:
  AppBannerDataFetcherDesktop(content::WebContents* web_contents,
                              base::WeakPtr<Delegate> weak_delegate,
                              int ideal_icon_size_in_dp,
                              int minimum_icon_size_in_dp,
                              bool is_debug_mode);

  // Callback for finishing bookmark app creation
  void FinishCreateBookmarkApp(const extensions::Extension* extension,
                               const WebApplicationInfo& web_app_info);

 protected:
  ~AppBannerDataFetcherDesktop() override;

 private:
  // AppBannerDataFetcher override.
  bool IsWebAppInstalled(content::BrowserContext* browser_context,
                         const GURL& start_url) override;

  // AppBannerDataFetcher override.
  void ShowBanner(const SkBitmap* icon,
                  const base::string16& title,
                  const std::string& referrer) override;

  std::unique_ptr<extensions::BookmarkAppHelper> bookmark_app_helper_;

  DISALLOW_COPY_AND_ASSIGN(AppBannerDataFetcherDesktop);
};

}  // namespace banners

#endif  // CHROME_BROWSER_BANNERS_APP_BANNER_DATA_FETCHER_DESKTOP_H_
