// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/ios/web_favicon_driver.h"

#include "base/bind.h"
#include "components/favicon/core/favicon_url.h"
#include "components/favicon/ios/favicon_url_util.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/gfx/image/image.h"

DEFINE_WEB_STATE_USER_DATA_KEY(favicon::WebFaviconDriver);

namespace favicon {

// static
void WebFaviconDriver::CreateForWebState(
    web::WebState* web_state,
    FaviconService* favicon_service,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model) {
  if (FromWebState(web_state))
    return;

  web_state->SetUserData(UserDataKey(),
                         new WebFaviconDriver(web_state, favicon_service,
                                              history_service, bookmark_model));
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

int WebFaviconDriver::StartDownload(const GURL& url, int max_image_size) {
  if (WasUnableToDownloadFavicon(url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << url;
    return 0;
  }

  return web_state()->DownloadImage(
      url, true, max_image_size, false,
      base::Bind(&FaviconDriverImpl::DidDownloadFavicon,
                 base::Unretained(this)));
}

bool WebFaviconDriver::IsOffTheRecord() {
  DCHECK(web_state());
  return web_state()->GetBrowserState()->IsOffTheRecord();
}

GURL WebFaviconDriver::GetActiveURL() {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  return item ? item->GetURL() : GURL();
}

base::string16 WebFaviconDriver::GetActiveTitle() {
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetVisibleItem();
  return item ? item->GetTitle() : base::string16();
}

bool WebFaviconDriver::GetActiveFaviconValidity() {
  return GetFaviconStatus().valid;
}

void WebFaviconDriver::SetActiveFaviconValidity(bool validity) {
  GetFaviconStatus().valid = validity;
}

GURL WebFaviconDriver::GetActiveFaviconURL() {
  return GetFaviconStatus().url;
}

void WebFaviconDriver::SetActiveFaviconURL(const GURL& url) {
  GetFaviconStatus().url = url;
}

gfx::Image WebFaviconDriver::GetActiveFaviconImage() {
  return GetFaviconStatus().image;
}

void WebFaviconDriver::SetActiveFaviconImage(const gfx::Image& image) {
  GetFaviconStatus().image = image;
}

web::FaviconStatus& WebFaviconDriver::GetFaviconStatus() {
  DCHECK(web_state()->GetNavigationManager()->GetVisibleItem());
  return web_state()->GetNavigationManager()->GetVisibleItem()->GetFavicon();
}

WebFaviconDriver::WebFaviconDriver(web::WebState* web_state,
                                   FaviconService* favicon_service,
                                   history::HistoryService* history_service,
                                   bookmarks::BookmarkModel* bookmark_model)
    : web::WebStateObserver(web_state),
      FaviconDriverImpl(favicon_service, history_service, bookmark_model) {
}

WebFaviconDriver::~WebFaviconDriver() {
}

void WebFaviconDriver::FaviconUrlUpdated(
    const std::vector<web::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());
  OnUpdateFaviconURL(FaviconURLsFromWebFaviconURLs(candidates));
}

}  // namespace favicon
