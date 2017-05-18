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
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_thread.h"
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
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(UserDataKey(), base::WrapUnique(new WebFaviconDriver(
                                            web_state, favicon_service,
                                            history_service, bookmark_model)));
}

void WebFaviconDriver::FetchFavicon(const GURL& url) {
  fetch_favicon_url_ = url;
  FaviconDriverImpl::FetchFavicon(url);
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
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetURL() != page_url)
    return;

  NotifyFaviconUpdatedObservers(notification_icon_type, icon_url,
                                icon_url_changed, image);
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model)
    : web::WebStateObserver(web_state),
      FaviconDriverImpl(favicon_service, history_service, bookmark_model),
      image_fetcher_(web_state->GetBrowserState()->GetRequestContext(),
                     web::WebThread::GetBlockingPool()) {}

WebFaviconDriver::~WebFaviconDriver() {
}

void WebFaviconDriver::FaviconUrlUpdated(
    const std::vector<web::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  OnUpdateCandidates(GetActiveURL(), FaviconURLsFromWebFaviconURLs(candidates),
                     GURL());
}

}  // namespace favicon
