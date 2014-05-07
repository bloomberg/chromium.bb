// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class holds the URL to an image and the bitmap for the fetched image,
// and has code to fetch the bitmap from the URL.

#include "chrome/browser/image_holder.h"

#include "chrome/browser/profiles/profile.h"
#include "net/base/load_flags.h"

namespace chrome {

ImageHolder::ImageHolder(const GURL& low_dpi_url,
                         const GURL& high_dpi_url,
                         Profile* profile,
                         ImageHolderDelegate* delegate)
    : low_dpi_url_(low_dpi_url),
      high_dpi_url_(high_dpi_url),
      low_dpi_fetched_(false),
      high_dpi_fetched_(false),
      delegate_(delegate),
      profile_(profile) {

  // If a URL is invalid, clear it so we don't try to fetch it.
  if (!low_dpi_url_.is_valid()) {
    low_dpi_url_ = GURL();
  }
  if (!high_dpi_url_.is_valid()) {
    high_dpi_url_ = GURL();
  }

  // Create a featcher for each URL that is set.
  if (!low_dpi_url_.is_empty()) {
    CreateBitmapFetcher(low_dpi_url_);
  }
  if (!high_dpi_url_.is_empty()) {
    CreateBitmapFetcher(high_dpi_url_);
  }
}

ImageHolder::~ImageHolder() {}

// This will let us know if we have tried to fetch once and the try completed.
// Currently there is no logic for retries.
bool ImageHolder::IsFetchingDone() const {
  return ((low_dpi_url_.is_empty() || low_dpi_fetched_) &&
           (high_dpi_url_.is_empty() || high_dpi_fetched_));
}

// If this bitmap has a valid GURL, create a fetcher for it.
void ImageHolder::CreateBitmapFetcher(const GURL& url) {
  // Check for dups, ignore any request for a dup.
  ScopedVector<chrome::BitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    if ((*iter)->url() == url)
      return;
  }

  if (url.is_valid()) {
    fetchers_.push_back(new chrome::BitmapFetcher(url, this));
    DVLOG(2) << __FUNCTION__ << "Pushing bitmap " << url;
  }
}

void ImageHolder::StartFetch() {
  // Now that we have queued them all, start the fetching.
  ScopedVector<chrome::BitmapFetcher>::iterator iter;
  for (iter = fetchers_.begin(); iter != fetchers_.end(); ++iter) {
    (*iter)->Start(
        profile_->GetRequestContext(),
        std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
  }
}

// Method inherited from BitmapFetcher delegate.
void ImageHolder::OnFetchComplete(const GURL url, const SkBitmap* bitmap) {
  // TODO(petewil): Should I retry if a fetch fails?
  // Match the bitmap to the URL to put it into the image with the correct scale
  // factor.
  if (url == low_dpi_url_) {
    low_dpi_fetched_ = true;
    if (bitmap != NULL)
      image_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, 1.0));
  } else if (url == high_dpi_url_) {
    high_dpi_fetched_ = true;
    if (bitmap != NULL)
      image_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, 2.0));
  } else {
    DVLOG(2) << __FUNCTION__ << "Unmatched bitmap arrived " << url;
  }

  // Notify callback of bitmap arrival.
  delegate_->OnFetchComplete();
}

}  // namespace chrome.
