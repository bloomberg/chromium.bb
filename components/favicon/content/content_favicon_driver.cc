// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include "base/bind.h"
#include "components/favicon/content/favicon_url_util.h"
#include "components/favicon/core/favicon_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/favicon_url.h"
#include "ui/gfx/image/image.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(favicon::ContentFaviconDriver);

namespace favicon {

// static
void ContentFaviconDriver::CreateForWebContents(
    content::WebContents* web_contents,
    FaviconService* favicon_service,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(), new ContentFaviconDriver(web_contents, favicon_service,
                                              history_service, bookmark_model));
}

gfx::Image ContentFaviconDriver::GetFavicon() const {
  // Like GetTitle(), we also want to use the favicon for the last committed
  // entry rather than a pending navigation entry.
  const content::NavigationController& controller =
      web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().image;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().image;
  return gfx::Image();
}

bool ContentFaviconDriver::FaviconIsValid() const {
  const content::NavigationController& controller =
      web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetTransientEntry();
  if (entry)
    return entry->GetFavicon().valid;

  entry = controller.GetLastCommittedEntry();
  if (entry)
    return entry->GetFavicon().valid;

  return false;
}

int ContentFaviconDriver::StartDownload(const GURL& url, int max_image_size) {
  if (WasUnableToDownloadFavicon(url)) {
    DVLOG(1) << "Skip Failed FavIcon: " << url;
    return 0;
  }

  bool bypass_cache = (bypass_cache_page_url_ == GetActiveURL());
  bypass_cache_page_url_ = GURL();

  return web_contents()->DownloadImage(
      url, true, max_image_size, bypass_cache,
      base::Bind(&FaviconDriverImpl::DidDownloadFavicon,
                 base::Unretained(this)));
}

bool ContentFaviconDriver::IsOffTheRecord() {
  DCHECK(web_contents());
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

GURL ContentFaviconDriver::GetActiveURL() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry ? entry->GetURL() : GURL();
}

base::string16 ContentFaviconDriver::GetActiveTitle() {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  return entry ? entry->GetTitle() : base::string16();
}

bool ContentFaviconDriver::GetActiveFaviconValidity() {
  return GetFaviconStatus().valid;
}

void ContentFaviconDriver::SetActiveFaviconValidity(bool valid) {
  GetFaviconStatus().valid = valid;
}

GURL ContentFaviconDriver::GetActiveFaviconURL() {
  return GetFaviconStatus().url;
}

void ContentFaviconDriver::SetActiveFaviconURL(const GURL& url) {
  GetFaviconStatus().url = url;
}

gfx::Image ContentFaviconDriver::GetActiveFaviconImage() {
  return GetFaviconStatus().image;
}

void ContentFaviconDriver::SetActiveFaviconImage(const gfx::Image& image) {
  GetFaviconStatus().image = image;
}

content::FaviconStatus& ContentFaviconDriver::GetFaviconStatus() {
  DCHECK(web_contents()->GetController().GetLastCommittedEntry());
  return web_contents()->GetController().GetLastCommittedEntry()->GetFavicon();
}

ContentFaviconDriver::ContentFaviconDriver(
    content::WebContents* web_contents,
    FaviconService* favicon_service,
    history::HistoryService* history_service,
    bookmarks::BookmarkModel* bookmark_model)
    : content::WebContentsObserver(web_contents),
      FaviconDriverImpl(favicon_service, history_service, bookmark_model) {
}

ContentFaviconDriver::~ContentFaviconDriver() {
}

void ContentFaviconDriver::NotifyFaviconUpdated(bool icon_url_changed) {
  FaviconDriverImpl::NotifyFaviconUpdated(icon_url_changed);
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
}

void ContentFaviconDriver::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK(!candidates.empty());

  // Ignore the update if there is no last committed navigation entry. This can
  // occur when loading an initially blank page.
  if (!web_contents()->GetController().GetLastCommittedEntry())
    return;

  favicon_urls_ = candidates;
  OnUpdateFaviconURL(FaviconURLsFromContentFaviconURLs(candidates));
}

void ContentFaviconDriver::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  if (reload_type == content::NavigationController::NO_RELOAD ||
      IsOffTheRecord())
    return;

  bypass_cache_page_url_ = url;
  SetFaviconOutOfDateForPage(
      url, reload_type == content::NavigationController::RELOAD_IGNORING_CACHE);
}

void ContentFaviconDriver::DidNavigateMainFrame(
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

}  // namespace favicon
