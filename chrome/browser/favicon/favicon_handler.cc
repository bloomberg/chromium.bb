// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_handler.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "components/favicon/core/browser/favicon_client.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon_base/favicon_util.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

using favicon::FaviconURL;

namespace {

// Size (along each axis) of a touch icon. This currently corresponds to
// the apple touch icon for iPad.
const int kTouchIconSize = 144;

// Get the maximal icon size in pixels for a icon of type |icon_type| for the
// current platform.
int GetMaximalIconSize(favicon_base::IconType icon_type) {
  switch (icon_type) {
    case favicon_base::FAVICON:
#if defined(OS_ANDROID)
      return 192;
#else
      return gfx::ImageSkia::GetMaxSupportedScale() * gfx::kFaviconSize;
#endif
    case favicon_base::TOUCH_ICON:
    case favicon_base::TOUCH_PRECOMPOSED_ICON:
      return kTouchIconSize;
    case favicon_base::INVALID_ICON:
      return 0;
  }
  NOTREACHED();
  return 0;
}

bool DoUrlAndIconMatch(const FaviconURL& favicon_url,
                       const GURL& url,
                       favicon_base::IconType icon_type) {
  return favicon_url.icon_url == url && favicon_url.icon_type == icon_type;
}

// Returns true if all of the icon URLs and icon types in |bitmap_results| are
// identical and if they match the icon URL and icon type in |favicon_url|.
// Returns false if |bitmap_results| is empty.
bool DoUrlsAndIconsMatch(
    const FaviconURL& favicon_url,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  if (bitmap_results.empty())
    return false;

  const favicon_base::IconType icon_type = favicon_url.icon_type;

  for (size_t i = 0; i < bitmap_results.size(); ++i) {
    if (favicon_url.icon_url != bitmap_results[i].icon_url ||
        icon_type != bitmap_results[i].icon_type) {
      return false;
    }
  }
  return true;
}

std::string UrlWithoutFragment(const GURL& gurl) {
  GURL::Replacements replacements;
  replacements.ClearRef();
  return gurl.ReplaceComponents(replacements).spec();
}

bool UrlMatches(const GURL& gurl_a, const GURL& gurl_b) {
  return UrlWithoutFragment(gurl_a) == UrlWithoutFragment(gurl_b);
}

// Return true if |bitmap_result| is expired.
bool IsExpired(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.expired;
}

// Return true if |bitmap_result| is valid.
bool IsValid(const favicon_base::FaviconRawBitmapResult& bitmap_result) {
  return bitmap_result.is_valid();
}

// Returns true if at least one of the bitmaps in |bitmap_results| is expired or
// if |bitmap_results| is missing favicons for |desired_size_in_dip| and one of
// the scale factors in favicon_base::GetFaviconScales().
bool HasExpiredOrIncompleteResult(
    int desired_size_in_dip,
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  // Check if at least one of the bitmaps is expired.
  std::vector<favicon_base::FaviconRawBitmapResult>::const_iterator it =
      std::find_if(bitmap_results.begin(), bitmap_results.end(), IsExpired);
  if (it != bitmap_results.end())
    return true;

  // Any favicon size is good if the desired size is 0.
  if (desired_size_in_dip == 0)
    return false;

  // Check if the favicon for at least one of the scale factors is missing.
  // |bitmap_results| should always be complete for data inserted by
  // FaviconHandler as the FaviconHandler stores favicons resized to all
  // of favicon_base::GetFaviconScales() into the history backend.
  // Examples of when |bitmap_results| can be incomplete:
  // - Favicons inserted into the history backend by sync.
  // - Favicons for imported bookmarks.
  std::vector<gfx::Size> favicon_sizes;
  for (size_t i = 0; i < bitmap_results.size(); ++i)
    favicon_sizes.push_back(bitmap_results[i].pixel_size);

  std::vector<float> favicon_scales = favicon_base::GetFaviconScales();
  for (size_t i = 0; i < favicon_scales.size(); ++i) {
    int edge_size_in_pixel = std::ceil(desired_size_in_dip * favicon_scales[i]);
    std::vector<gfx::Size>::iterator it = std::find(favicon_sizes.begin(),
        favicon_sizes.end(), gfx::Size(edge_size_in_pixel, edge_size_in_pixel));
    if (it == favicon_sizes.end())
      return true;
  }
  return false;
}

// Returns true if at least one of |bitmap_results| is valid.
bool HasValidResult(
    const std::vector<favicon_base::FaviconRawBitmapResult>& bitmap_results) {
  return std::find_if(bitmap_results.begin(), bitmap_results.end(), IsValid) !=
      bitmap_results.end();
}

// Returns the index of the entry with the largest area that is not larger than
// |max_area|; -1 if there is no such match.
int GetLargestSizeIndex(const std::vector<gfx::Size>& sizes, int max_area) {
  DCHECK(!sizes.empty());
  int ret = -1;
  for (size_t i = 0; i < sizes.size(); ++i) {
    int area = sizes[i].GetArea();
    if ((ret == -1 || sizes[ret].GetArea() < area) && area <= max_area)
      ret = i;
  }
  return ret;
}

// Return the index of a size which is same as the given |size|, -1 returned if
// there is no such bitmap.
int GetIndexBySize(const std::vector<gfx::Size>& sizes,
                   const gfx::Size& size) {
  DCHECK(!sizes.empty());
  std::vector<gfx::Size>::const_iterator i =
      std::find(sizes.begin(), sizes.end(), size);
  if (i == sizes.end())
    return -1;

  return static_cast<int>(i - sizes.begin());
}

// Compare function used for std::stable_sort to sort as descend.
bool CompareIconSize(const FaviconURL& b1, const FaviconURL& b2) {
  int area1 = 0;
  if (!b1.icon_sizes.empty())
    area1 = b1.icon_sizes.front().GetArea();

  int area2 = 0;
  if (!b2.icon_sizes.empty())
    area2 = b2.icon_sizes.front().GetArea();

  return area1 > area2;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::DownloadRequest::DownloadRequest()
    : icon_type(favicon_base::INVALID_ICON) {}

FaviconHandler::DownloadRequest::~DownloadRequest() {
}

FaviconHandler::DownloadRequest::DownloadRequest(
    const GURL& url,
    const GURL& image_url,
    favicon_base::IconType icon_type)
    : url(url), image_url(image_url), icon_type(icon_type) {}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconCandidate::FaviconCandidate()
    : score(0), icon_type(favicon_base::INVALID_ICON) {}

FaviconHandler::FaviconCandidate::~FaviconCandidate() {
}

FaviconHandler::FaviconCandidate::FaviconCandidate(
    const GURL& url,
    const GURL& image_url,
    const gfx::Image& image,
    float score,
    favicon_base::IconType icon_type)
    : url(url),
      image_url(image_url),
      image(image),
      score(score),
      icon_type(icon_type) {}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconHandler(FaviconClient* client,
                               FaviconDriver* driver,
                               Type icon_type,
                               bool download_largest_icon)
    : got_favicon_from_history_(false),
      favicon_expired_or_incomplete_(false),
      icon_types_(icon_type == FAVICON
                      ? favicon_base::FAVICON
                      : favicon_base::TOUCH_ICON |
                            favicon_base::TOUCH_PRECOMPOSED_ICON),
      download_largest_icon_(download_largest_icon),
      client_(client),
      driver_(driver) {
  DCHECK(driver_);
}

FaviconHandler::~FaviconHandler() {
}

void FaviconHandler::FetchFavicon(const GURL& url) {
  cancelable_task_tracker_.TryCancelAll();

  url_ = url;

  favicon_expired_or_incomplete_ = got_favicon_from_history_ = false;
  image_urls_.clear();

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well WebContents) when the favicon url is
  // available.
  if (client_->GetFaviconService()) {
    GetFaviconForURLFromFaviconService(
        url_,
        icon_types_,
        base::Bind(
            &FaviconHandler::OnFaviconDataForInitialURLFromFaviconService,
            base::Unretained(this)),
        &cancelable_task_tracker_);
  }
}

bool FaviconHandler::UpdateFaviconCandidate(const GURL& url,
                                            const GURL& image_url,
                                            const gfx::Image& image,
                                            float score,
                                            favicon_base::IconType icon_type) {
  bool replace_best_favicon_candidate = false;
  bool exact_match = false;
  if (download_largest_icon_) {
    replace_best_favicon_candidate =
        image.Size().GetArea() >
        best_favicon_candidate_.image.Size().GetArea();

    gfx::Size largest = best_favicon_candidate_.image.Size();
    if (replace_best_favicon_candidate)
      largest = image.Size();

    // exact match (stop downloading next icon) only if
    // - current candidate is only candidate.
    // - next candidate doesn't have sizes attributes, in this case, the rest
    //   candidates don't have sizes attribute either, stop downloading now,
    //   otherwise, all favicon without sizes attribute are downloaded.
    // - next candidate has sizes attribute and it is not larger than largest.
    exact_match = image_urls_.size() == 1 ||
        image_urls_[1].icon_sizes.empty() ||
        image_urls_[1].icon_sizes[0].GetArea() < largest.GetArea();
  } else {
    exact_match = score == 1 || preferred_icon_size() == 0;
    replace_best_favicon_candidate =
        exact_match ||
        best_favicon_candidate_.icon_type == favicon_base::INVALID_ICON ||
        score > best_favicon_candidate_.score;
  }
  if (replace_best_favicon_candidate) {
    best_favicon_candidate_ = FaviconCandidate(
        url, image_url, image, score, icon_type);
  }
  return exact_match;
}

void FaviconHandler::SetFavicon(const GURL& url,
                                const GURL& icon_url,
                                const gfx::Image& image,
                                favicon_base::IconType icon_type) {
  if (client_->GetFaviconService() && ShouldSaveFavicon(url))
    SetHistoryFavicons(url, icon_url, icon_type, image);

  if (UrlMatches(url, url_) && icon_type == favicon_base::FAVICON) {
    if (!PageChangedSinceFaviconWasRequested())
      SetFaviconOnActivePage(icon_url, image);
  }
}

void FaviconHandler::SetFaviconOnActivePage(const std::vector<
    favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results) {
  gfx::Image resized_image = favicon_base::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results,
      favicon_base::GetFaviconScales(),
      preferred_icon_size());
  // The history service sends back results for a single icon URL, so it does
  // not matter which result we get the |icon_url| from.
  const GURL icon_url = favicon_bitmap_results.empty() ?
      GURL() : favicon_bitmap_results[0].icon_url;
  SetFaviconOnActivePage(icon_url, resized_image);
}

void FaviconHandler::SetFaviconOnActivePage(const GURL& icon_url,
                                            const gfx::Image& image) {
  // No matter what happens, we need to mark the favicon as being set.
  driver_->SetActiveFaviconValidity(true);

  bool icon_url_changed = driver_->GetActiveFaviconURL() != icon_url;
  driver_->SetActiveFaviconURL(icon_url);

  if (image.IsEmpty())
    return;

  gfx::Image image_with_adjusted_colorspace = image;
  favicon_base::SetFaviconColorSpace(&image_with_adjusted_colorspace);

  driver_->SetActiveFaviconImage(image_with_adjusted_colorspace);
  NotifyFaviconUpdated(icon_url_changed);
}

void FaviconHandler::OnUpdateFaviconURL(
    const std::vector<FaviconURL>& candidates) {
  image_urls_.clear();
  best_favicon_candidate_ = FaviconCandidate();
  for (std::vector<FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_empty() && (i->icon_type & icon_types_))
      image_urls_.push_back(*i);
  }

  if (!client_->GetFaviconService())
    return;

  if (download_largest_icon_)
    SortAndPruneImageUrls();

  // TODO(davemoore) Should clear on empty url. Currently we ignore it.
  // This appears to be what FF does as well.
  if (!image_urls_.empty())
    ProcessCurrentUrl();
}

void FaviconHandler::ProcessCurrentUrl() {
  DCHECK(!image_urls_.empty());

  // current_candidate() may return NULL if download_largest_icon_ is true and
  // all the sizes are larger than the max.
  if (PageChangedSinceFaviconWasRequested() || !current_candidate())
    return;

  if (current_candidate()->icon_type == favicon_base::FAVICON) {
    if (!favicon_expired_or_incomplete_ &&
        driver_->GetActiveFaviconValidity() &&
        DoUrlAndIconMatch(*current_candidate(),
                          driver_->GetActiveFaviconURL(),
                          favicon_base::FAVICON))
      return;
  } else if (!favicon_expired_or_incomplete_ && got_favicon_from_history_ &&
             HasValidResult(history_results_) &&
             DoUrlsAndIconsMatch(*current_candidate(), history_results_)) {
    return;
  }

  if (got_favicon_from_history_)
    DownloadFaviconOrAskFaviconService(driver_->GetActiveURL(),
                                       current_candidate()->icon_url,
                                       current_candidate()->icon_type);
}

void FaviconHandler::OnDidDownloadFavicon(
    int id,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  DownloadRequests::iterator i = download_requests_.find(id);
  if (i == download_requests_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  if (current_candidate() &&
      DoUrlAndIconMatch(*current_candidate(), image_url, i->second.icon_type)) {
    bool request_next_icon = true;
    float score = 0.0f;
    gfx::ImageSkia image_skia;
    if (download_largest_icon_ && !bitmaps.empty()) {
      int index = -1;
      int max_size = GetMaximalIconSize(i->second.icon_type);
      // Use the largest bitmap if FaviconURL doesn't have sizes attribute.
      if (current_candidate()->icon_sizes.empty()) {
        index = GetLargestSizeIndex(original_bitmap_sizes, max_size * max_size);
      } else {
        index = GetIndexBySize(original_bitmap_sizes,
                               current_candidate()->icon_sizes[0]);
        // Find largest bitmap if there is no one exactly matched.
        if (index == -1) {
          index = GetLargestSizeIndex(original_bitmap_sizes,
                                      max_size * max_size);
        }
      }
      if (index != -1)
        image_skia = gfx::ImageSkia(gfx::ImageSkiaRep(bitmaps[index], 1));
    } else {
      image_skia = CreateFaviconImageSkia(bitmaps,
                                          original_bitmap_sizes,
                                          preferred_icon_size(),
                                          &score);
    }

    if (!image_skia.isNull()) {
      gfx::Image image(image_skia);
      // The downloaded icon is still valid when there is no FaviconURL update
      // during the downloading.
      if (!bitmaps.empty()) {
        request_next_icon = !UpdateFaviconCandidate(
            i->second.url, image_url, image, score, i->second.icon_type);
      }
    }
    if (request_next_icon && !PageChangedSinceFaviconWasRequested() &&
        image_urls_.size() > 1) {
      // Remove the first member of image_urls_ and process the remaining.
      image_urls_.erase(image_urls_.begin());
      ProcessCurrentUrl();
    } else if (best_favicon_candidate_.icon_type !=
               favicon_base::INVALID_ICON) {
      // No more icons to request, set the favicon from the candidate.
      SetFavicon(best_favicon_candidate_.url,
                 best_favicon_candidate_.image_url,
                 best_favicon_candidate_.image,
                 best_favicon_candidate_.icon_type);
      // Reset candidate.
      image_urls_.clear();
      best_favicon_candidate_ = FaviconCandidate();
    }
  }
  download_requests_.erase(i);
}

bool FaviconHandler::PageChangedSinceFaviconWasRequested() {
  if (UrlMatches(driver_->GetActiveURL(), url_) && url_.is_valid()) {
    return false;
  }
  // If the URL has changed out from under us (as will happen with redirects)
  // return true.
  return true;
}

int FaviconHandler::DownloadFavicon(const GURL& image_url,
                                    int max_bitmap_size) {
  if (!image_url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  return driver_->StartDownload(image_url, max_bitmap_size);
}

void FaviconHandler::UpdateFaviconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(pkotwicz): pass in all of |image_urls_| to
  // UpdateFaviconMappingsAndFetch().
  std::vector<GURL> icon_urls;
  icon_urls.push_back(icon_url);
  client_->GetFaviconService()->UpdateFaviconMappingsAndFetch(
      page_url, icon_urls, icon_type, preferred_icon_size(), callback, tracker);
}

void FaviconHandler::GetFaviconFromFaviconService(
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  client_->GetFaviconService()->GetFavicon(
      icon_url, icon_type, preferred_icon_size(), callback, tracker);
}

void FaviconHandler::GetFaviconForURLFromFaviconService(
    const GURL& page_url,
    int icon_types,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  client_->GetFaviconService()->GetFaviconForPageURL(
      page_url,
      icon_types,
      preferred_icon_size(),
      callback,
      tracker);
}

void FaviconHandler::SetHistoryFavicons(const GURL& page_url,
                                        const GURL& icon_url,
                                        favicon_base::IconType icon_type,
                                        const gfx::Image& image) {
  client_->GetFaviconService()->SetFavicons(
      page_url, icon_url, icon_type, image);
}

bool FaviconHandler::ShouldSaveFavicon(const GURL& url) {
  if (!driver_->IsOffTheRecord())
    return true;

  // Otherwise store the favicon if the page is bookmarked.
  return client_->IsBookmarked(url);
}

void FaviconHandler::NotifyFaviconUpdated(bool icon_url_changed) {
  driver_->NotifyFaviconUpdated(icon_url_changed);
}

void FaviconHandler::OnFaviconDataForInitialURLFromFaviconService(
    const std::vector<favicon_base::FaviconRawBitmapResult>&
        favicon_bitmap_results) {
  if (PageChangedSinceFaviconWasRequested())
    return;
  got_favicon_from_history_ = true;
  history_results_ = favicon_bitmap_results;
  bool has_results = !favicon_bitmap_results.empty();
  favicon_expired_or_incomplete_ = has_results && HasExpiredOrIncompleteResult(
      preferred_icon_size(), favicon_bitmap_results);
  if (has_results && icon_types_ == favicon_base::FAVICON &&
      !driver_->GetActiveFaviconValidity() &&
      (!current_candidate() ||
       DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results))) {
    if (HasValidResult(favicon_bitmap_results)) {
      // The db knows the favicon (although it may be out of date) and the entry
      // doesn't have an icon. Set the favicon now, and if the favicon turns out
      // to be expired (or the wrong url) we'll fetch later on. This way the
      // user doesn't see a flash of the default favicon.
      SetFaviconOnActivePage(favicon_bitmap_results);
    } else {
      // If |favicon_bitmap_results| does not have any valid results, treat the
      // favicon as if it's expired.
      // TODO(pkotwicz): Do something better.
      favicon_expired_or_incomplete_ = true;
    }
  }
  if (has_results && !favicon_expired_or_incomplete_) {
    if (current_candidate() &&
        !DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results)) {
      // Mapping in the database is wrong. DownloadFavIconOrAskHistory will
      // update the mapping for this url and download the favicon if we don't
      // already have it.
      DownloadFaviconOrAskFaviconService(driver_->GetActiveURL(),
                                         current_candidate()->icon_url,
                                         current_candidate()->icon_type);
    }
  } else if (current_candidate()) {
    // We know the official url for the favicon, but either don't have the
    // favicon or it's expired. Continue on to DownloadFaviconOrAskHistory to
    // either download or check history again.
    DownloadFaviconOrAskFaviconService(driver_->GetActiveURL(),
                                       current_candidate()->icon_url,
                                       current_candidate()->icon_type);
  }
  // else we haven't got the icon url. When we get it we'll ask the
  // renderer to download the icon.
}

void FaviconHandler::DownloadFaviconOrAskFaviconService(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type) {
  if (favicon_expired_or_incomplete_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(page_url, icon_url, icon_type);
  } else if (client_->GetFaviconService()) {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (driver_->IsOffTheRecord()) {
      GetFaviconFromFaviconService(
          icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      UpdateFaviconMappingAndFetch(
          page_url, icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    }
  }
}

void FaviconHandler::OnFaviconData(const std::vector<
    favicon_base::FaviconRawBitmapResult>& favicon_bitmap_results) {
  if (PageChangedSinceFaviconWasRequested())
    return;

  bool has_results = !favicon_bitmap_results.empty();
  bool has_expired_or_incomplete_result = HasExpiredOrIncompleteResult(
      preferred_icon_size(), favicon_bitmap_results);

  if (has_results && icon_types_ == favicon_base::FAVICON) {
    if (HasValidResult(favicon_bitmap_results)) {
      // There is a favicon, set it now. If expired we'll download the current
      // one again, but at least the user will get some icon instead of the
      // default and most likely the current one is fine anyway.
      SetFaviconOnActivePage(favicon_bitmap_results);
    }
    if (has_expired_or_incomplete_result) {
      // The favicon is out of date. Request the current one.
      ScheduleDownload(driver_->GetActiveURL(),
                       driver_->GetActiveFaviconURL(),
                       favicon_base::FAVICON);
    }
  } else if (current_candidate() &&
      (!has_results || has_expired_or_incomplete_result ||
       !(DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results)))) {
    // We don't know the favicon, it is out of date or its type is not same as
    // one got from page. Request the current one.
    ScheduleDownload(driver_->GetActiveURL(),
                     current_candidate()->icon_url,
                     current_candidate()->icon_type);
  }
  history_results_ = favicon_bitmap_results;
}

int FaviconHandler::ScheduleDownload(const GURL& url,
                                     const GURL& image_url,
                                     favicon_base::IconType icon_type) {
  // A max bitmap size is specified to avoid receiving huge bitmaps in
  // OnDidDownloadFavicon(). See FaviconDriver::StartDownload()
  // for more details about the max bitmap size.
  const int download_id = DownloadFavicon(image_url,
                                          GetMaximalIconSize(icon_type));
  if (download_id) {
    // Download ids should be unique.
    DCHECK(download_requests_.find(download_id) == download_requests_.end());
    download_requests_[download_id] =
        DownloadRequest(url, image_url, icon_type);
  }

  return download_id;
}

void FaviconHandler::SortAndPruneImageUrls() {
  for (std::vector<FaviconURL>::iterator i = image_urls_.begin();
       i != image_urls_.end();) {
    if (i->icon_sizes.empty()) {
      ++i;
      continue;
    }
    int max_size = GetMaximalIconSize(i->icon_type);
    int index = GetLargestSizeIndex(i->icon_sizes, max_size * max_size);
    if (index == -1) {
      i = image_urls_.erase(i);
    } else {
      gfx::Size largest = i->icon_sizes[index];
      i->icon_sizes.clear();
      i->icon_sizes.push_back(largest);
      ++i;
    }
  }
  std::stable_sort(image_urls_.begin(), image_urls_.end(),
                   CompareIconSize);
}
