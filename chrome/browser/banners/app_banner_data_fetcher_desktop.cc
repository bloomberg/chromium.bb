// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"

#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/web_application_info.h"
#include "content/public/browser/render_frame_host.h"

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace banners {

AppBannerDataFetcherDesktop::AppBannerDataFetcherDesktop(
    content::WebContents* web_contents,
    base::WeakPtr<Delegate> weak_delegate,
    int ideal_icon_size_in_dp,
    int minimum_icon_size_in_dp,
    bool is_debug_mode)
    : AppBannerDataFetcher(web_contents,
                           weak_delegate,
                           ideal_icon_size_in_dp,
                           minimum_icon_size_in_dp,
                           is_debug_mode) {}

AppBannerDataFetcherDesktop::~AppBannerDataFetcherDesktop() {
}

bool AppBannerDataFetcherDesktop::IsWebAppInstalled(
    content::BrowserContext* browser_context,
    const GURL& start_url) {
  return extensions::BookmarkAppHelper::BookmarkOrHostedAppInstalled(
      browser_context, start_url);
}

void AppBannerDataFetcherDesktop::ShowBanner(const SkBitmap* icon,
                                             const base::string16& title,
                                             const std::string& referrer) {
  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents && !web_app_data().IsEmpty());

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  WebApplicationInfo web_app_info;

  bookmark_app_helper_.reset(
      new extensions::BookmarkAppHelper(profile, web_app_info, web_contents));

  // This differs from the Android infobar creation, which has an explicit
  // InfoBarAndroid class interfacing with Java. On Android, the data fetcher
  // calls the InfoBarService to show the banner. On desktop, an InfoBar class
  // is not required, so the InfoBarService call is made within the delegate.
  infobars::InfoBar* infobar = AppBannerInfoBarDelegateDesktop::Create(
      make_scoped_refptr(this), web_contents, web_app_data(),
      bookmark_app_helper_.get(), event_request_id());
  if (infobar) {
    RecordDidShowBanner("AppBanner.WebApp.Shown");
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
  }
}

void AppBannerDataFetcherDesktop::FinishCreateBookmarkApp(
    const extensions::Extension* extension,
    const WebApplicationInfo& web_app_info) {
  content::WebContents* web_contents = GetWebContents();
  if (web_contents) {
    // A null extension pointer indicates that the bookmark app install was
    // not successful.
    if (extension == nullptr) {
      web_contents->GetMainFrame()->Send(
          new ChromeViewMsg_AppBannerDismissed(
              web_contents->GetMainFrame()->GetRoutingID(),
              event_request_id()));

      AppBannerSettingsHelper::RecordBannerDismissEvent(
          web_contents, web_app_data().start_url.spec(),
          AppBannerSettingsHelper::WEB);
    } else {
      web_contents->GetMainFrame()->Send(
          new ChromeViewMsg_AppBannerAccepted(
              web_contents->GetMainFrame()->GetRoutingID(),
              event_request_id(), "web"));

      AppBannerSettingsHelper::RecordBannerInstallEvent(
          web_contents, web_app_data().start_url.spec(),
          AppBannerSettingsHelper::WEB);
    }
  }
}

}  // namespace banners
