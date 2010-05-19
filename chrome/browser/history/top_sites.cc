// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/history/page_usage_data.h"
#include "gfx/codec/jpeg_codec.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

// How many top sites to store in the cache.
static const int kTopSitesNumber = 20;
static const int kDaysOfHistory = 90;
static const int64 kUpdateIntervalSecs = 15;  // TODO(Nik): come up
                                              // with an algorithm for timing.

TopSites::TopSites(Profile* profile) : profile_(profile),
                                       mock_history_service_(NULL) {
}

TopSites::~TopSites() {
}

void TopSites::Init() {
  // TODO(brettw) read the database here.

  // Start the one-shot timer.
  timer_.Start(base::TimeDelta::FromSeconds(kUpdateIntervalSecs), this,
               &TopSites::StartQueryForMostVisited);
}

bool TopSites::SetPageThumbnail(const GURL& url,
                                const SkBitmap& thumbnail,
                                const ThumbnailScore& score) {
  AutoLock lock(lock_);

  std::map<GURL, size_t>::iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return false;  // This URL is not known to us.
  MostVisitedURL& most_visited = top_sites_[found->second];
  Images& image = top_images_[most_visited.url];

  // When comparing the thumbnail scores, we need to take into account the
  // redirect hops, which are not generated when the thumbnail is becuase the
  // redirects weren't known. We fill that in here since we know the redirects.
  ThumbnailScore new_score_with_redirects(score);
  new_score_with_redirects.redirect_hops_from_dest =
      GetRedirectDistanceForURL(most_visited, url);

  if (!ShouldReplaceThumbnailWith(image.thumbnail_score,
                                  new_score_with_redirects))
    return false;  // The one we already have is better.

  image.thumbnail = new RefCountedBytes;
  SkAutoLockPixels thumbnail_lock(thumbnail);
  bool encoded = gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(thumbnail.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_BGRA, thumbnail.width(),
      thumbnail.height(),
      static_cast<int>(thumbnail.rowBytes()), 90,
      &image.thumbnail->data);
  if (!encoded)
    return false;
  image.thumbnail_score = new_score_with_redirects;

  return true;
}

MostVisitedURLList TopSites::GetMostVisitedURLs() {
  AutoLock lock(lock_);
  return top_sites_;
}

bool TopSites::GetPageThumbnail(const GURL& url, RefCountedBytes** data) const {
  std::map<GURL, Images>::const_iterator found = top_images_.find(url);
  if (found == top_images_.end())
    return false;  // No thumbnail for this URL.

  Images image = found->second;
  *data = image.thumbnail.get();
  return true;
}

void TopSites::StoreMostVisited(MostVisitedURLList* most_visited) {
  lock_.AssertAcquired();
  // TODO(brettw) filter for blacklist!

  if (!top_sites_.empty()) {
    std::vector<size_t> added;
    std::vector<size_t> deleted;
    std::vector<size_t> moved;
    DiffMostVisited(top_sites_, *most_visited, &added, &deleted, &moved);

    // Delete all the thumbnails associated with URLs that were deleted.
    for (size_t i = 0; i < deleted.size(); i++) {
      std::map<GURL, Images>::iterator found =
          top_images_.find(top_sites_[deleted[i]].url);
      if (found != top_images_.end())
        top_images_.erase(found);
    }

    // TODO(brettw) write the new data to disk.
  }

  // Take ownership of the most visited data.
  top_sites_.clear();
  top_sites_.swap(*most_visited);

  // Save the redirect information for quickly mapping to the canonical URLs.
  canonical_urls_.clear();
  for (size_t i = 0; i < top_sites_.size(); i++)
    StoreRedirectChain(top_sites_[i].redirects, i);
}

void TopSites::StoreRedirectChain(const RedirectList& redirects,
                                  size_t destination) {
  lock_.AssertAcquired();

  if (redirects.empty()) {
    NOTREACHED();
    return;
  }

  // We shouldn't get any duplicates.
  DCHECK(canonical_urls_.find(redirects[0]) == canonical_urls_.end());

  // Map all the redirected URLs to the destination.
  for (size_t i = 0; i < redirects.size(); i++)
    canonical_urls_[redirects[i]] = destination;
}

GURL TopSites::GetCanonicalURL(const GURL& url) const {
  lock_.AssertAcquired();

  std::map<GURL, size_t>::const_iterator found = canonical_urls_.find(url);
  if (found == canonical_urls_.end())
    return GURL();  // Don't know anything about this URL.
  return top_sites_[found->second].url;
}

// static
int TopSites::GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                        const GURL& url) {
  for (size_t i = 0; i < most_visited.redirects.size(); i++) {
    if (most_visited.redirects[i] == url)
      return static_cast<int>(most_visited.redirects.size() - i - 1);
  }
  NOTREACHED() << "URL should always be found.";
  return 0;
}

// static
void TopSites::DiffMostVisited(const MostVisitedURLList& old_list,
                               const MostVisitedURLList& new_list,
                               std::vector<size_t>* added_urls,
                               std::vector<size_t>* deleted_urls,
                               std::vector<size_t>* moved_urls) {
  added_urls->clear();
  deleted_urls->clear();
  moved_urls->clear();

  // Add all the old URLs for quick lookup. This maps URLs to the corresponding
  // index in the input.
  std::map<GURL, size_t> all_old_urls;
  for (size_t i = 0; i < old_list.size(); i++)
    all_old_urls[old_list[i].url] = i;

  // Check all the URLs in the new set to see which ones are new or just moved.
  // When we find a match in the old set, we'll reset its index to our special
  // marker. This allows us to quickly identify the deleted ones in a later
  // pass.
  const size_t kAlreadyFoundMarker = static_cast<size_t>(-1);
  for (size_t i = 0; i < new_list.size(); i++) {
    std::map<GURL, size_t>::iterator found = all_old_urls.find(new_list[i].url);
    if (found == all_old_urls.end()) {
      added_urls->push_back(i);
    } else {
      if (found->second != i)
        moved_urls->push_back(i);
      found->second = kAlreadyFoundMarker;
    }
  }

  // Any member without the special marker in the all_old_urls list means that
  // there wasn't a "new" URL that mapped to it, so it was deleted.
  for (std::map<GURL, size_t>::const_iterator i = all_old_urls.begin();
       i != all_old_urls.end(); ++i) {
    if (i->second != kAlreadyFoundMarker)
      deleted_urls->push_back(i->second);
  }
}

void TopSites::StartQueryForMostVisited() {
  if (mock_history_service_) {
    // Testing with a mockup.
    // QuerySegmentUsageSince is not virtual, so we have to duplicate the code.
    mock_history_service_->QueryMostVisitedURLs(
        kTopSitesNumber,
        kDaysOfHistory,
        &cancelable_consumer_,
        NewCallback(this, &TopSites::OnTopSitesAvailable));
  } else {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    // |hs| may be null during unit tests.
    if (hs) {
      hs->QueryMostVisitedURLs(
          kTopSitesNumber,
          kDaysOfHistory,
          &cancelable_consumer_,
          NewCallback(this, &TopSites::OnTopSitesAvailable));
    } else {
      LOG(INFO) << "History Service not available.";
    }
  }
}

void TopSites::OnTopSitesAvailable(
    CancelableRequestProvider::Handle handle,
    MostVisitedURLList pages) {
  AutoLock lock(lock_);
  StoreMostVisited(&pages);
}

void TopSites::SetMockHistoryService(MockHistoryService* mhs) {
  mock_history_service_ = mhs;
}

}  // namespace history
