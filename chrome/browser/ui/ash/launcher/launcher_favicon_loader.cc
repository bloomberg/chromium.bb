// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_favicon_loader.h"

#include "ash/shelf/shelf_constants.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace internal {

const int kMaxBitmapSize = 256;

////////////////////////////////////////////////////////////////////////////////
// FaviconRawBitmapHandler fetchs all bitmaps with the 'icon' (or 'shortcut
// icon')
// link tag, storing the one that best matches ash::kShelfSize.
// These icon bitmaps are not resized and are not cached beyond the lifetime
// of the class. Bitmaps larger than kMaxBitmapSize are ignored.

class FaviconRawBitmapHandler : public content::WebContentsObserver {
 public:
  FaviconRawBitmapHandler(content::WebContents* web_contents,
                          LauncherFaviconLoader::Delegate* delegate)
      : content::WebContentsObserver(web_contents),
        delegate_(delegate),
        weak_ptr_factory_(this) {}

  virtual ~FaviconRawBitmapHandler() {}

  const SkBitmap& bitmap() const { return bitmap_; }

  bool HasPendingDownloads() const;

  // content::WebContentObserver implementation.
  virtual void DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) OVERRIDE;

 private:
  void DidDownloadFavicon(
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  void AddFavicon(const GURL& image_url, const SkBitmap& new_bitmap);

  LauncherFaviconLoader::Delegate* delegate_;

  typedef std::set<GURL> UrlSet;
  // Map of pending download urls.
  UrlSet pending_requests_;
  // Map of processed urls.
  UrlSet processed_requests_;
  // Current bitmap and source url.
  SkBitmap bitmap_;
  GURL bitmap_url_;

  base::WeakPtrFactory<FaviconRawBitmapHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaviconRawBitmapHandler);
};

void FaviconRawBitmapHandler::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // This function receives a complete list of faviocn urls for the page.
  // It may get called multiple times with the same list, and will also get
  // called any time an item is added or removed. As such, we track processed
  // and pending urls, but only until they are removed from the list.
  UrlSet new_pending, new_processed;
  // Create a map of valid favicon urls.
  std::set<GURL> urls;
  std::vector<content::FaviconURL>::const_iterator iter;
  for (iter = candidates.begin(); iter != candidates.end(); ++iter) {
    if (iter->icon_type != content::FaviconURL::FAVICON)
      continue;
    const GURL& url = iter->icon_url;
    if (url.is_valid())
      urls.insert(url);
    // Preserve matching pending requests amd processed requests.
    if (pending_requests_.find(url) != pending_requests_.end())
      new_pending.insert(url);
    if (processed_requests_.find(url) != processed_requests_.end())
      new_processed.insert(url);
  }
  pending_requests_ = new_pending;
  processed_requests_ = new_processed;
  // Reset bitmap_ if no longer valid (i.e. not in the list of urls).
  if (urls.find(bitmap_url_) == urls.end()) {
    bitmap_url_ = GURL();
    bitmap_.reset();
  }
  // Request any new urls.
  for (std::set<GURL>::iterator iter = urls.begin();
       iter != urls.end(); ++iter) {
    if (processed_requests_.find(*iter) != processed_requests_.end())
      continue;  // Skip already processed downloads.
    if (pending_requests_.find(*iter) != pending_requests_.end())
      continue;  // Skip already pending downloads.
    pending_requests_.insert(*iter);
    web_contents()->DownloadImage(
        *iter,
        true,  // is a favicon
        0,     // no maximum size
        base::Bind(&FaviconRawBitmapHandler::DidDownloadFavicon,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

bool FaviconRawBitmapHandler::HasPendingDownloads() const {
  return !pending_requests_.empty();
}

void FaviconRawBitmapHandler::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  UrlSet::iterator iter = pending_requests_.find(image_url);
  if (iter == pending_requests_.end()) {
    // Updates are received for all downloads; ignore unrequested urls.
    return;
  }
  pending_requests_.erase(iter);

  // Favicon bitmaps are ordered by decreasing width.
  if (!bitmaps.empty())
    AddFavicon(image_url, bitmaps[0]);
}

void FaviconRawBitmapHandler::AddFavicon(const GURL& image_url,
                                         const SkBitmap& new_bitmap) {
  processed_requests_.insert(image_url);
  if (new_bitmap.height() > kMaxBitmapSize ||
      new_bitmap.width() > kMaxBitmapSize)
    return;
  if (new_bitmap.height() < ash::kShelfSize)
    return;
  if (!bitmap_.isNull()) {
    // We want the smallest icon that is large enough.
    if (new_bitmap.height() > bitmap_.height())
      return;
  }
  bitmap_url_ = image_url;
  bitmap_ = new_bitmap;
  delegate_->FaviconUpdated();
}

}  // namespace internal

////////////////////////////////////////////////////////////////////////////////

LauncherFaviconLoader::LauncherFaviconLoader(Delegate* delegate,
                                             content::WebContents* web_contents)
    : web_contents_(web_contents) {
  favicon_handler_.reset(
      new internal::FaviconRawBitmapHandler(web_contents, delegate));
}

LauncherFaviconLoader::~LauncherFaviconLoader() {
}

SkBitmap LauncherFaviconLoader::GetFavicon() const {
  return favicon_handler_->bitmap();
}

bool LauncherFaviconLoader::HasPendingDownloads() const {
  return favicon_handler_->HasPendingDownloads();
}
