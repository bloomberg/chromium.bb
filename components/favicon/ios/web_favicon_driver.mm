// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon/ios/favicon_url_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/load_committed_details.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/navigation_context.h"
#include "ios/web/public/web_state/web_state.h"
#include "skia/ext/skia_utils_ios.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(favicon::WebFaviconDriver);

// Callback for the download of favicon.
using ImageDownloadCallback =
    base::Callback<void(int image_id,
                        int http_status_code,
                        const GURL& image_url,
                        const std::vector<SkBitmap>& bitmaps,
                        const std::vector<gfx::Size>& sizes)>;

namespace favicon {

// static
void WebFaviconDriver::CreateForWebState(
    web::WebState* web_state,
    FaviconService* favicon_service,
    history::HistoryService* history_service) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(UserDataKey(),
                         base::WrapUnique(new WebFaviconDriver(
                             web_state, favicon_service, history_service)));
}

gfx::Image WebFaviconDriver::GetFavicon() const {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().image : gfx::Image();
}

bool WebFaviconDriver::FaviconIsValid() const {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetLastCommittedItem();
  return item ? item->GetFavicon().valid : false;
}

GURL WebFaviconDriver::GetActiveURL() {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  return item ? item->GetURL() : GURL();
}

int WebFaviconDriver::DownloadImage(const GURL& url,
                                    int max_image_size,
                                    ImageDownloadCallback callback) {
  static int downloaded_image_count = 0;
  int local_download_id = ++downloaded_image_count;

  GURL local_url(url);

  image_fetcher::IOSImageDataFetcherCallback local_callback =
      ^(NSData* data, const image_fetcher::RequestMetadata& metadata) {
        if (metadata.http_response_code ==
            image_fetcher::RequestMetadata::RESPONSE_CODE_INVALID)
          return;

        std::vector<SkBitmap> frames;
        std::vector<gfx::Size> sizes;
        if (data) {
          frames = skia::ImageDataToSkBitmaps(data);
          for (const auto& frame : frames) {
            sizes.push_back(gfx::Size(frame.width(), frame.height()));
          }
        }
        callback.Run(local_download_id, metadata.http_response_code, local_url,
                     frames, sizes);
      };
  image_fetcher_.FetchImageDataWebpDecoded(url, local_callback);

  return downloaded_image_count;
}

void WebFaviconDriver::DownloadManifest(const GURL& url,
                                        ManifestDownloadCallback callback) {
  NOTREACHED();
}

bool WebFaviconDriver::IsOffTheRecord() {
  DCHECK(web_state());
  return web_state()->GetBrowserState()->IsOffTheRecord();
}

void WebFaviconDriver::OnFaviconUpdated(
    const GURL& page_url,
    FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  // Check whether the active URL has changed since FetchFavicon() was called.
  // On iOS, the active URL can change between calls to FetchFavicon(). For
  // instance, FetchFavicon() is not synchronously called when the active URL
  // changes as a result of CRWSessionController::goToEntry().
  if (GetActiveURL() != page_url && !page_url.is_empty()) {
    return;
  }

  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  DCHECK(item);

  web::FaviconStatus& favicon_status = item->GetFavicon();
  favicon_status.valid = true;
  favicon_status.image = image;

  NotifyFaviconUpdatedObservers(notification_icon_type, icon_url,
                                icon_url_changed, image);
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   FaviconService* favicon_service,
                                   history::HistoryService* history_service)
    : web::WebStateObserver(web_state),
      FaviconDriverImpl(favicon_service, history_service),
      image_fetcher_(web_state->GetBrowserState()->GetRequestContext()) {}

WebFaviconDriver::~WebFaviconDriver() {
}

void WebFaviconDriver::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  SetFaviconOutOfDateForPage(navigation_context->GetUrl(),
                             /*force_reload=*/false);
}

void WebFaviconDriver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetError())
    return;

  // Fetch the favicon for the new URL.
  FetchFavicon(navigation_context->GetUrl(),
               navigation_context->IsSameDocument());

  if (navigation_context->IsSameDocument()) {
    if (!candidates_.empty()) {
      FaviconUrlUpdatedInternal(candidates_);
    }
  } else {
    candidates_.clear();
  }
}

void WebFaviconDriver::FaviconUrlUpdated(
    web::WebState* web_state,
    const std::vector<web::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  candidates_ = FaviconURLsFromWebFaviconURLs(candidates);
  FaviconUrlUpdatedInternal(candidates_);
}

void WebFaviconDriver::FaviconUrlUpdatedInternal(
    const std::vector<favicon::FaviconURL>& candidates) {
  OnUpdateCandidates(GetActiveURL(), candidates, GURL());
}

}  // namespace favicon
