// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager_desktop.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/banners/app_banner_infobar_delegate_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "extensions/common/constants.h"

namespace {

bool gDisableTriggeringForTesting = false;

}  // namespace

namespace banners {

bool AppBannerManagerDesktop::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kAppBanners) ||
         IsExperimentalAppBannersEnabled();
}

// static
AppBannerManager* AppBannerManager::FromWebContents(
    content::WebContents* web_contents) {
  return AppBannerManagerDesktop::FromWebContents(web_contents);
}

void AppBannerManagerDesktop::DisableTriggeringForTesting() {
  gDisableTriggeringForTesting = true;
}

AppBannerManagerDesktop::AppBannerManagerDesktop(
    content::WebContents* web_contents)
    : AppBannerManager(web_contents) { }

AppBannerManagerDesktop::~AppBannerManagerDesktop() { }

void AppBannerManagerDesktop::CreateWebApp(WebappInstallSource install_source) {
  content::WebContents* contents = web_contents();
  DCHECK(contents);

  // TODO(loyso): Take appropriate action if WebApps disabled for profile.
  web_app::CreateWebAppFromBanner(
      contents, install_source,
      base::BindOnce(&AppBannerManager::DidFinishCreatingWebApp, GetWeakPtr()));
}

void AppBannerManagerDesktop::DidFinishCreatingWebApp(
    const web_app::AppId& app_id,
    web_app::InstallResultCode code) {
  content::WebContents* contents = web_contents();
  if (!contents)
    return;

  // BookmarkAppInstallManager returns kFailedUnknownReason for any error.
  // We can't distinguish kUserInstallDeclined case so far.
  // If kFailedUnknownReason, we assume that the confirmation dialog was
  // cancelled. Alternatively, the web app installation may have failed, but
  // we can't tell the difference here.
  // TODO(crbug.com/789381): plumb through enough information to be able to
  // distinguish between extension install failures and user-cancellations of
  // the app install dialog.
  if (code != web_app::InstallResultCode::kSuccess) {
    SendBannerDismissed();
    TrackUserResponse(USER_RESPONSE_WEB_APP_DISMISSED);
    AppBannerSettingsHelper::RecordBannerDismissEvent(
        contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);
    return;
  }

  SendBannerAccepted();
  AppBannerSettingsHelper::RecordBannerInstallEvent(
      contents, GetAppIdentifier(), AppBannerSettingsHelper::WEB);

  // OnInstall must be called last since it resets Mojo bindings.
  OnInstall(false /* is_native app */, blink::kWebDisplayModeStandalone);
}

bool AppBannerManagerDesktop::IsWebAppConsideredInstalled(
    content::WebContents* web_contents,
    const GURL& validated_url,
    const GURL& start_url,
    const GURL& manifest_url) {
  return extensions::BookmarkOrHostedAppInstalled(
      web_contents->GetBrowserContext(), start_url);
}

void AppBannerManagerDesktop::ShowBannerUi(WebappInstallSource install_source) {
  if (IsExperimentalAppBannersEnabled()) {
    RecordDidShowBanner("AppBanner.WebApp.Shown");
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
    TrackUserResponse(USER_RESPONSE_WEB_APP_ACCEPTED);
    ReportStatus(SHOWING_APP_INSTALLATION_DIALOG);
    CreateWebApp(install_source);
    return;
  }

  content::WebContents* contents = web_contents();
  DCHECK(contents && !manifest_.IsEmpty());

  // This differs from Android, where there is a concrete
  // AppBannerInfoBarAndroid class to interface with Java, and the manager calls
  // the InfoBarService to show the banner. On desktop, an InfoBar class
  // is not required, and the delegate calls the InfoBarService.
  infobars::InfoBar* infobar = AppBannerInfoBarDelegateDesktop::Create(
      contents, GetWeakPtr(), install_source, manifest_);
  if (infobar) {
    RecordDidShowBanner("AppBanner.WebApp.Shown");
    TrackDisplayEvent(DISPLAY_EVENT_WEB_APP_BANNER_CREATED);
    ReportStatus(SHOWING_WEB_APP_BANNER);
  } else {
    ReportStatus(FAILED_TO_CREATE_BANNER);
  }
}

void AppBannerManagerDesktop::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::DidFinishLoad(render_frame_host, validated_url);
}

void AppBannerManagerDesktop::OnEngagementEvent(
    content::WebContents* web_contents,
    const GURL& url,
    double score,
    SiteEngagementService::EngagementType type) {
  if (gDisableTriggeringForTesting)
    return;

  AppBannerManager::OnEngagementEvent(web_contents, url, score, type);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppBannerManagerDesktop)

}  // namespace banners
