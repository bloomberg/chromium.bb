// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/favicon_downloader.h"

#include "base/bind.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/favicon_url.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

FaviconDownloader::FaviconDownloader(
  content::WebContents* web_contents,
  const std::vector<GURL>& extra_favicon_urls,
  FaviconDownloaderCallback callback)
  : content::WebContentsObserver(web_contents),
    need_favicon_urls_(true),
    extra_favicon_urls_(extra_favicon_urls),
    callback_(callback),
    weak_ptr_factory_(this) {
}

FaviconDownloader::~FaviconDownloader() {
}

void FaviconDownloader::SkipPageFavicons() {
  need_favicon_urls_ = false;
}

void FaviconDownloader::Start() {
  FetchIcons(extra_favicon_urls_);
  // If the candidates aren't loaded, icons will be fetched when
  // DidUpdateFaviconURL() is called.

  if (need_favicon_urls_) {
    std::vector<content::FaviconURL> favicon_tab_helper_urls =
        GetFaviconURLsFromWebContents();
    if (!favicon_tab_helper_urls.empty()) {
      need_favicon_urls_ = false;
      FetchIcons(favicon_tab_helper_urls);
    }
  }
}

int FaviconDownloader::DownloadImage(const GURL& url) {
  return web_contents()->DownloadImage(
      url,
      true,  // is_favicon
      0,     // no max size
      false,  // normal cache policy
      base::Bind(&FaviconDownloader::DidDownloadFavicon,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::vector<content::FaviconURL>
    FaviconDownloader::GetFaviconURLsFromWebContents() {
  favicon::ContentFaviconDriver* content_favicon_driver =
      web_contents()
          ? favicon::ContentFaviconDriver::FromWebContents(web_contents())
          : nullptr;
  // If favicon_urls() is empty, we are guaranteed that DidUpdateFaviconURLs has
  // not yet been called for the current page's navigation.
  return content_favicon_driver ? content_favicon_driver->favicon_urls()
                                : std::vector<content::FaviconURL>();
}

void FaviconDownloader::FetchIcons(
    const std::vector<content::FaviconURL>& favicon_urls) {
  std::vector<GURL> urls;
  for (std::vector<content::FaviconURL>::const_iterator it =
           favicon_urls.begin();
       it != favicon_urls.end(); ++it) {
    if (it->icon_type != content::FaviconURL::INVALID_ICON)
      urls.push_back(it->icon_url);
  }
  FetchIcons(urls);
}

void FaviconDownloader::FetchIcons(const std::vector<GURL>& urls) {
  // Download icons; put their download ids into |in_progress_requests_| and
  // their urls into |processed_urls_|.
  for (std::vector<GURL>::const_iterator it = urls.begin();
       it != urls.end(); ++it) {
    // Only start the download if the url hasn't been processed before.
    if (processed_urls_.insert(*it).second)
      in_progress_requests_.insert(DownloadImage(*it));
  }

  // If no downloads were initiated, we can proceed directly to running the
  // callback.
  if (in_progress_requests_.empty() && !need_favicon_urls_)
    callback_.Run(true, favicon_map_);
}

void FaviconDownloader::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  // Request may have been canceled by DidFinishNavigation().
  if (in_progress_requests_.erase(id) == 0)
    return;

  favicon_map_[image_url] = bitmaps;

  // Once all requests have been resolved, perform post-download tasks.
  if (in_progress_requests_.empty() && !need_favicon_urls_)
    callback_.Run(true, favicon_map_);
}

// content::WebContentsObserver overrides:
void FaviconDownloader::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  // Clear all pending requests.
  in_progress_requests_.clear();
  favicon_map_.clear();
  callback_.Run(false, favicon_map_);
}

void FaviconDownloader::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // Only consider the first candidates we are given. This prevents pages that
  // change their favicon from spamming us.
  if (!need_favicon_urls_)
    return;

  need_favicon_urls_ = false;
  FetchIcons(candidates);
}
