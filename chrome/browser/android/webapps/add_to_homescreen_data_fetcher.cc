// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/add_to_homescreen_data_fetcher.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/strings/string16.h"
#include "chrome/browser/android/offline_pages/offline_page_utils.h"
#include "chrome/browser/android/shortcut_helper.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/web_application_info.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/manifest.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace {

// Looks up the original, online URL of the site requested.  The URL from the
// WebContents may be an offline page or a distilled article which is not
// appropriate for a home screen shortcut.
GURL GetShortcutUrl(content::BrowserContext* browser_context,
                    const GURL& actual_url) {
  GURL original_url =
      dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(actual_url);

  // If URL points to an offline content, get original URL.
  GURL online_url =
      offline_pages::OfflinePageUtils::MaybeGetOnlineURLForOfflineURL(
          browser_context, original_url);
  if (online_url.is_valid())
    return online_url;

  return original_url;
}

InstallableParams ParamsToPerformInstallableCheck(int ideal_icon_size_in_dp,
                                                  int minimum_icon_size_in_dp) {
  // TODO(hanxi): change check_installable to true for WebAPKs.
  InstallableParams params;
  params.ideal_icon_size_in_dp = ideal_icon_size_in_dp;
  params.minimum_icon_size_in_dp = minimum_icon_size_in_dp;
  params.check_installable = false;
  params.fetch_valid_icon = true;
  return params;
}

}  // namespace

AddToHomescreenDataFetcher::AddToHomescreenDataFetcher(
    content::WebContents* web_contents,
    int ideal_icon_size_in_dp,
    int minimum_icon_size_in_dp,
    int ideal_splash_image_size_in_dp,
    int minimum_splash_image_size_in_dp,
    Observer* observer)
    : WebContentsObserver(web_contents),
      weak_observer_(observer),
      shortcut_info_(GetShortcutUrl(web_contents->GetBrowserContext(),
                                    web_contents->GetLastCommittedURL())),
      data_timeout_timer_(false, false),
      ideal_icon_size_in_dp_(ideal_icon_size_in_dp),
      minimum_icon_size_in_dp_(minimum_icon_size_in_dp),
      ideal_splash_image_size_in_dp_(ideal_splash_image_size_in_dp),
      minimum_splash_image_size_in_dp_(minimum_splash_image_size_in_dp),
      is_waiting_for_web_application_info_(true),
      is_icon_saved_(false),
      is_ready_(false) {
  DCHECK(minimum_icon_size_in_dp <= ideal_icon_size_in_dp);
  DCHECK(minimum_splash_image_size_in_dp <= ideal_splash_image_size_in_dp);

  // Send a message to the renderer to retrieve information about the page.
  Send(new ChromeViewMsg_GetWebApplicationInfo(routing_id()));
}

void AddToHomescreenDataFetcher::OnDidGetWebApplicationInfo(
    const WebApplicationInfo& received_web_app_info) {
  is_waiting_for_web_application_info_ = false;
  if (!web_contents() || !weak_observer_)
    return;

  // Sanitize received_web_app_info.
  WebApplicationInfo web_app_info = received_web_app_info;
  web_app_info.title =
      web_app_info.title.substr(0, chrome::kMaxMetaTagAttributeLength);
  web_app_info.description =
      web_app_info.description.substr(0, chrome::kMaxMetaTagAttributeLength);

  // Simply set the user-editable title to be the page's title
  shortcut_info_.user_title = web_app_info.title.empty()
                                  ? web_contents()->GetTitle()
                                  : web_app_info.title;
  shortcut_info_.short_name = shortcut_info_.user_title;
  shortcut_info_.name = shortcut_info_.user_title;

  if (web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE ||
      web_app_info.mobile_capable == WebApplicationInfo::MOBILE_CAPABLE_APPLE) {
    shortcut_info_.display = blink::WebDisplayModeStandalone;
  }

  // Record what type of shortcut was added by the user.
  switch (web_app_info.mobile_capable) {
    case WebApplicationInfo::MOBILE_CAPABLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcut"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_APPLE:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.AppShortcutApple"));
      break;
    case WebApplicationInfo::MOBILE_CAPABLE_UNSPECIFIED:
      content::RecordAction(
          base::UserMetricsAction("webapps.AddShortcut.Bookmark"));
      break;
  }

  InstallableManager::CreateForWebContents(web_contents());
  InstallableManager* manager =
      InstallableManager::FromWebContents(web_contents());
  DCHECK(manager);

  // Kick off a timeout for downloading data. If we haven't finished within the
  // timeout, fall back to using a dynamically-generated launcher icon.
  data_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(4000),
      base::Bind(&AddToHomescreenDataFetcher::OnFaviconFetched, this,
                  favicon_base::FaviconRawBitmapResult()));

  manager->GetData(
      ParamsToPerformInstallableCheck(ideal_icon_size_in_dp_,
                                      minimum_icon_size_in_dp_),
      base::Bind(&AddToHomescreenDataFetcher::OnDidPerformInstallableCheck,
                 this));
}

void AddToHomescreenDataFetcher::OnDidPerformInstallableCheck(
    const InstallableData& data) {
  if (!web_contents() || !weak_observer_)
    return;

  if (!data.manifest.IsEmpty()) {
    content::RecordAction(
        base::UserMetricsAction("webapps.AddShortcut.Manifest"));
    shortcut_info_.UpdateFromManifest(data.manifest);
    shortcut_info_.manifest_url = data.manifest_url;
  }

  // Save the splash screen URL for the later download.
  splash_screen_url_ = ManifestIconSelector::FindBestMatchingIcon(
      data.manifest.icons, ideal_splash_image_size_in_dp_,
      minimum_splash_image_size_in_dp_);

  weak_observer_->OnUserTitleAvailable(shortcut_info_.user_title);

  if (data.icon) {
    // TODO(hanxi): implement WebAPK path if shortcut_info_.url has a secure
    // scheme and data.is_installable is true.
    shortcut_info_.icon_url = data.icon_url;

    // base::Bind copies the arguments internally, so it is safe to pass it
    // data.icon (which is not owned by us).
    content::BrowserThread::GetBlockingPool()
        ->PostWorkerTaskWithShutdownBehavior(
            FROM_HERE,
            base::Bind(
                &AddToHomescreenDataFetcher::CreateLauncherIconInBackground,
                this, *(data.icon)),
            base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
    return;
  }

  FetchFavicon();
}

bool AddToHomescreenDataFetcher::OnMessageReceived(
    const IPC::Message& message) {
  if (!is_waiting_for_web_application_info_)
    return false;

  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(AddToHomescreenDataFetcher, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DidGetWebApplicationInfo,
                        OnDidGetWebApplicationInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

AddToHomescreenDataFetcher::~AddToHomescreenDataFetcher() {
  DCHECK(!weak_observer_);
}

base::Closure AddToHomescreenDataFetcher::FetchSplashScreenImageCallback(
    const std::string& webapp_id) {
  return base::Bind(&ShortcutHelper::FetchSplashScreenImage, web_contents(),
                    splash_screen_url_, ideal_splash_image_size_in_dp_,
                    minimum_splash_image_size_in_dp_, webapp_id);
}

void AddToHomescreenDataFetcher::FetchFavicon() {
  if (!web_contents() || !weak_observer_)
    return;

  // Grab the best, largest icon we can find to represent this bookmark.
  // TODO(dfalcantara): Try combining with the new BookmarksHandler once its
  //                    rewrite is further along.
  std::vector<int> icon_types{
      favicon_base::FAVICON,
      favicon_base::TOUCH_PRECOMPOSED_ICON | favicon_base::TOUCH_ICON};

  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()),
          ServiceAccessType::EXPLICIT_ACCESS);

  // Using favicon if its size is not smaller than platform required size,
  // otherwise using the largest icon among all avaliable icons.
  int ideal_icon_size_in_px =
      ideal_icon_size_in_dp_ *
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  int threshold_to_get_any_largest_icon = ideal_icon_size_in_px - 1;
  favicon_service->GetLargestRawFaviconForPageURL(
      shortcut_info_.url, icon_types, threshold_to_get_any_largest_icon,
      base::Bind(&AddToHomescreenDataFetcher::OnFaviconFetched, this),
      &favicon_task_tracker_);
}

void AddToHomescreenDataFetcher::OnFaviconFetched(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  if (!web_contents() || !weak_observer_ || is_icon_saved_)
    return;

  content::BrowserThread::GetBlockingPool()->PostWorkerTaskWithShutdownBehavior(
      FROM_HERE, base::Bind(&AddToHomescreenDataFetcher::
                                CreateLauncherIconFromFaviconInBackground,
                            this, bitmap_result),
      base::SequencedWorkerPool::SKIP_ON_SHUTDOWN);
}

void AddToHomescreenDataFetcher::CreateLauncherIconFromFaviconInBackground(
    const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  SkBitmap raw_icon;
  if (bitmap_result.is_valid()) {
    gfx::PNGCodec::Decode(bitmap_result.bitmap_data->front(),
                          bitmap_result.bitmap_data->size(), &raw_icon);
  }

  shortcut_info_.icon_url = bitmap_result.icon_url;
  CreateLauncherIconInBackground(raw_icon);
}

void AddToHomescreenDataFetcher::CreateLauncherIconInBackground(
    const SkBitmap& raw_icon) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  SkBitmap icon;
  bool is_generated = false;
  if (weak_observer_) {
    icon = weak_observer_->FinalizeLauncherIconInBackground(
        raw_icon, shortcut_info_.url, &is_generated);
  }

  if (is_generated)
    shortcut_info_.icon_url = GURL();

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AddToHomescreenDataFetcher::NotifyObserver, this, icon));
}

void AddToHomescreenDataFetcher::NotifyObserver(const SkBitmap& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents() || !weak_observer_ || is_icon_saved_)
    return;

  is_icon_saved_ = true;
  shortcut_icon_ = icon;
  is_ready_ = true;
  weak_observer_->OnDataAvailable(shortcut_info_, shortcut_icon_);
}
