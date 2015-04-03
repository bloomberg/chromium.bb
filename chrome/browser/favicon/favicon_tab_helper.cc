// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_tab_helper.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/favicon/content/favicon_url_util.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/favicon_url.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using content::FaviconStatus;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(FaviconTabHelper);

namespace {

// Returns whether icon NTP is enabled by experiment.
// TODO(huangs): Remove all 3 copies of this routine once Icon NTP launches.
bool IsIconNTPEnabled() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name = base::FieldTrialList::FindFullName("IconNTP");
  using base::CommandLine;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableIconNtp))
    return false;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableIconNtp))
    return true;

  return StartsWithASCII(group_name, "Enabled", true);
}

#if defined(OS_ANDROID) || defined(OS_IOS)
const bool kDownloadLargestIcon = true;
const bool kEnableTouchIcon = true;
#else
const bool kDownloadLargestIcon = false;
const bool kEnableTouchIcon = false;
#endif

}  // namespace

// static
void FaviconTabHelper::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (FromWebContents(web_contents))
    return;

  Profile* original_profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetOriginalProfile();
  web_contents->SetUserData(
      UserDataKey(),
      new FaviconTabHelper(
          web_contents,
          FaviconServiceFactory::GetForProfile(
              original_profile, ServiceAccessType::IMPLICIT_ACCESS),
          HistoryServiceFactory::GetForProfile(
              original_profile, ServiceAccessType::IMPLICIT_ACCESS),
          BookmarkModelFactory::GetForProfileIfExists(original_profile)));
}

FaviconTabHelper::FaviconTabHelper(WebContents* web_contents,
                                   favicon::FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model)
    : content::WebContentsObserver(web_contents),
      favicon_service_(favicon_service),
      history_service_(history_service),
      bookmark_model_(bookmark_model) {
  favicon_handler_.reset(new favicon::FaviconHandler(
      favicon_service_, this, favicon::FaviconHandler::FAVICON,
      kDownloadLargestIcon));
  if (kEnableTouchIcon) {
    touch_icon_handler_.reset(new favicon::FaviconHandler(
        favicon_service_, this, favicon::FaviconHandler::TOUCH,
        kDownloadLargestIcon));
  }
  if (IsIconNTPEnabled()) {
    large_icon_handler_.reset(new favicon::FaviconHandler(
        favicon_service_, this, favicon::FaviconHandler::LARGE, true));
  }
}

FaviconTabHelper::~FaviconTabHelper() {
}

void FaviconTabHelper::FetchFavicon(const GURL& url) {
  favicon_handler_->FetchFavicon(url);
  if (touch_icon_handler_.get())
    touch_icon_handler_->FetchFavicon(url);
  if (large_icon_handler_.get())
    large_icon_handler_->FetchFavicon(url);
}

gfx::Image FaviconTabHelper::GetFavicon() const {
  // Like GetTitle(), we also want to use the favicon for the last committed
  // entry rather than a pending navigation entry.
  const NavigationController& controller = web_contents()->GetController();
  NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().image;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().image;
  return gfx::Image();
}

bool FaviconTabHelper::FaviconIsValid() const {
  const NavigationController& controller = web_contents()->GetController();
  NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().valid;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().valid;

  return false;
}

bool FaviconTabHelper::ShouldDisplayFavicon() {
  // Always display a throbber during pending loads.
  const NavigationController& controller = web_contents()->GetController();
  if (controller.GetLastCommittedEntry() && controller.GetPendingEntry())
    return true;

  GURL url = web_contents()->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) &&
      url.host() == chrome::kChromeUINewTabHost) {
    return false;
  }

  // No favicon on Instant New Tab Pages.
  if (chrome::IsInstantNTP(web_contents()))
    return false;

  return true;
}

void FaviconTabHelper::SaveFavicon() {
  GURL active_url = GetActiveURL();
  if (active_url.is_empty())
    return;

  // Make sure the page is in history, otherwise adding the favicon does
  // nothing.
  if (!history_service_)
    return;
  history_service_->AddPageNoVisitForBookmark(active_url, GetActiveTitle());

  if (!favicon_service_)
    return;
  if (!GetActiveFaviconValidity())
    return;
  GURL favicon_url = GetActiveFaviconURL();
  if (favicon_url.is_empty())
    return;
  gfx::Image image = GetActiveFaviconImage();
  if (image.IsEmpty())
    return;
  favicon_service_->SetFavicons(active_url, favicon_url, favicon_base::FAVICON,
                                image);
}

void FaviconTabHelper::AddObserver(favicon::FaviconDriverObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FaviconTabHelper::RemoveObserver(
    favicon::FaviconDriverObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

int FaviconTabHelper::StartDownload(const GURL& url, int max_image_size) {
  if (favicon_service_ && favicon_service_->WasUnableToDownloadFavicon(url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << url;
    return 0;
  }

  bool bypass_cache = (bypass_cache_page_url_ == GetActiveURL());
  bypass_cache_page_url_ = GURL();

  return web_contents()->DownloadImage(
      url, true, max_image_size, bypass_cache,
      base::Bind(&FaviconTabHelper::DidDownloadFavicon,
                 base::Unretained(this)));
}

bool FaviconTabHelper::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

bool FaviconTabHelper::IsBookmarked(const GURL& url) {
  return bookmark_model_ && bookmark_model_->IsBookmarked(url);
}

GURL FaviconTabHelper::GetActiveURL() {
  NavigationEntry* entry = web_contents()->GetController().GetActiveEntry();
  return entry ? entry->GetURL() : GURL();
}

base::string16 FaviconTabHelper::GetActiveTitle() {
  NavigationEntry* entry = web_contents()->GetController().GetActiveEntry();
  return entry ? entry->GetTitle() : base::string16();
}

bool FaviconTabHelper::GetActiveFaviconValidity() {
  return GetFaviconStatus().valid;
}

void FaviconTabHelper::SetActiveFaviconValidity(bool valid) {
  GetFaviconStatus().valid = valid;
}

GURL FaviconTabHelper::GetActiveFaviconURL() {
  return GetFaviconStatus().url;
}

void FaviconTabHelper::SetActiveFaviconURL(const GURL& url) {
  GetFaviconStatus().url = url;
}

gfx::Image FaviconTabHelper::GetActiveFaviconImage() {
  return GetFaviconStatus().image;
}

void FaviconTabHelper::SetActiveFaviconImage(const gfx::Image& image) {
  GetFaviconStatus().image = image;
}

void FaviconTabHelper::OnFaviconAvailable(const gfx::Image& image,
                                          const GURL& icon_url,
                                          bool is_active_favicon) {
  if (is_active_favicon) {
    // No matter what happens, we need to mark the favicon as being set.
    SetActiveFaviconValidity(true);
    bool icon_url_changed = GetActiveFaviconURL() != icon_url;
    SetActiveFaviconURL(icon_url);

    if (image.IsEmpty())
      return;

    SetActiveFaviconImage(image);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_FAVICON_UPDATED,
        content::Source<WebContents>(web_contents()),
        content::Details<bool>(&icon_url_changed));
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
  if (!image.IsEmpty()) {
    FOR_EACH_OBSERVER(favicon::FaviconDriverObserver, observer_list_,
                      OnFaviconAvailable(image));
  }
}

void FaviconTabHelper::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  if (bitmaps.empty() && http_status_code == 404) {
    DVLOG(1) << "Failed to Download Favicon:" << image_url;
    if (favicon_service_)
      favicon_service_->UnableToDownloadFavicon(image_url);
  }

  favicon_handler_->OnDidDownloadFavicon(id, image_url, bitmaps,
                                         original_bitmap_sizes);
  if (touch_icon_handler_.get()) {
    touch_icon_handler_->OnDidDownloadFavicon(id, image_url, bitmaps,
                                              original_bitmap_sizes);
  }
  if (large_icon_handler_.get()) {
    large_icon_handler_->OnDidDownloadFavicon(id, image_url, bitmaps,
                                              original_bitmap_sizes);
  }
}

content::FaviconStatus& FaviconTabHelper::GetFaviconStatus() {
  DCHECK(web_contents()->GetController().GetActiveEntry());
  return web_contents()->GetController().GetActiveEntry()->GetFavicon();
}

void FaviconTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    NavigationController::ReloadType reload_type) {
  if (reload_type == NavigationController::NO_RELOAD || IsOffTheRecord())
    return;

  bypass_cache_page_url_ = url;

  if (favicon_service_) {
    favicon_service_->SetFaviconOutOfDateForPage(url);
    if (reload_type == NavigationController::RELOAD_IGNORING_CACHE)
      favicon_service_->ClearUnableToDownloadFavicons();
  }
}

void FaviconTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  favicon_urls_.clear();

  // Wait till the user navigates to a new URL to start checking the cache
  // again. The cache may be ignored for non-reload navigations (e.g.
  // history.replace() in-page navigation). This is allowed to increase the
  // likelihood that "reloading a page ignoring the cache" redownloads the
  // favicon. In particular, a page may do an in-page navigation before
  // FaviconHandler has the time to determine that the favicon needs to be
  // redownloaded.
  GURL url = details.entry->GetURL();
  if (url != bypass_cache_page_url_)
    bypass_cache_page_url_ = GURL();

  // Get the favicon, either from history or request it from the net.
  FetchFavicon(url);
}

void FaviconTabHelper::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  favicon_urls_ = candidates;
  std::vector<favicon::FaviconURL> favicon_urls =
      favicon::FaviconURLsFromContentFaviconURLs(candidates);
  favicon_handler_->OnUpdateFaviconURL(favicon_urls);
  if (touch_icon_handler_.get())
    touch_icon_handler_->OnUpdateFaviconURL(favicon_urls);
  if (large_icon_handler_.get())
    large_icon_handler_->OnUpdateFaviconURL(favicon_urls);
}
