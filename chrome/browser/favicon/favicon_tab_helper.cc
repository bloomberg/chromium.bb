// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_tab_helper.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_handler.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

using content::FaviconStatus;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(FaviconTabHelper);

FaviconTabHelper::FaviconTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  favicon_handler_.reset(
      new FaviconHandler(profile_, this, this, FaviconHandler::FAVICON));
  if (chrome::kEnableTouchIcon)
    touch_icon_handler_.reset(
        new FaviconHandler(profile_, this, this, FaviconHandler::TOUCH));
}

FaviconTabHelper::~FaviconTabHelper() {
}

void FaviconTabHelper::FetchFavicon(const GURL& url) {
  favicon_handler_->FetchFavicon(url);
  if (touch_icon_handler_.get())
    touch_icon_handler_->FetchFavicon(url);
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
  NavigationEntry* entry = web_contents()->GetController().GetActiveEntry();
  if (!entry || entry->GetURL().is_empty())
    return;

  // Make sure the page is in history, otherwise adding the favicon does
  // nothing.
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_->GetOriginalProfile(), Profile::IMPLICIT_ACCESS);
  if (!history)
    return;
  history->AddPageNoVisitForBookmark(entry->GetURL(), entry->GetTitle());

  FaviconService* service = FaviconServiceFactory::GetForProfile(
      profile_->GetOriginalProfile(), Profile::IMPLICIT_ACCESS);
  if (!service)
    return;
  const FaviconStatus& favicon(entry->GetFavicon());
  if (!favicon.valid || favicon.url.is_empty() ||
      favicon.image.IsEmpty()) {
    return;
  }
  service->SetFavicons(
      entry->GetURL(), favicon.url, chrome::FAVICON, favicon.image);
}

NavigationEntry* FaviconTabHelper::GetActiveEntry() {
  return web_contents()->GetController().GetActiveEntry();
}

int FaviconTabHelper::StartDownload(const GURL& url, int max_image_size) {
  FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
      profile_->GetOriginalProfile(), Profile::IMPLICIT_ACCESS);
  if (favicon_service && favicon_service->WasUnableToDownloadFavicon(url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << url;
    return 0;
  }

  return web_contents()->DownloadImage(
      url,
      true,
      max_image_size,
      base::Bind(&FaviconTabHelper::DidDownloadFavicon,base::Unretained(this)));
}

void FaviconTabHelper::NotifyFaviconUpdated(bool icon_url_changed) {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<WebContents>(web_contents()),
      content::Details<bool>(&icon_url_changed));
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}

bool FaviconTabHelper::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

void FaviconTabHelper::DidStartNavigationToPendingEntry(
    const GURL& url,
    NavigationController::ReloadType reload_type) {
  if (reload_type != NavigationController::NO_RELOAD &&
      !profile_->IsOffTheRecord()) {
    FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
        profile_, Profile::IMPLICIT_ACCESS);
    if (favicon_service) {
      favicon_service->SetFaviconOutOfDateForPage(url);
      if (reload_type == NavigationController::RELOAD_IGNORING_CACHE)
        favicon_service->ClearUnableToDownloadFavicons();
    }
  }
}

void FaviconTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  favicon_urls_.clear();
  // Get the favicon, either from history or request it from the net.
  FetchFavicon(details.entry->GetURL());
}

void FaviconTabHelper::DidUpdateFaviconURL(
    int32 page_id,
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  favicon_urls_ = candidates;

  favicon_handler_->OnUpdateFaviconURL(page_id, candidates);
  if (touch_icon_handler_.get())
    touch_icon_handler_->OnUpdateFaviconURL(page_id, candidates);
}

FaviconService* FaviconTabHelper::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(profile_,
                                              Profile::EXPLICIT_ACCESS);
}

void FaviconTabHelper::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {

  if (bitmaps.empty() && http_status_code == 404) {
    DVLOG(1) << "Failed to Download Favicon:" << image_url;
    FaviconService* favicon_service = FaviconServiceFactory::GetForProfile(
        profile_->GetOriginalProfile(), Profile::IMPLICIT_ACCESS);
    if (favicon_service)
      favicon_service->UnableToDownloadFavicon(image_url);
  }

  favicon_handler_->OnDidDownloadFavicon(
      id, image_url, bitmaps, original_bitmap_sizes);
  if (touch_icon_handler_.get()) {
    touch_icon_handler_->OnDidDownloadFavicon(
        id, image_url, bitmaps, original_bitmap_sizes);
  }
}
