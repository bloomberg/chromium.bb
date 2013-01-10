// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_handler.h"

#include "build/build_config.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/history/select_favicon_frames.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

using content::FaviconURL;
using content::NavigationEntry;

namespace {

// Returns history::IconType the given icon_type corresponds to.
history::IconType ToHistoryIconType(FaviconURL::IconType icon_type) {
  switch (icon_type) {
    case FaviconURL::FAVICON:
      return history::FAVICON;
    case FaviconURL::TOUCH_ICON:
      return history::TOUCH_ICON;
    case FaviconURL::TOUCH_PRECOMPOSED_ICON:
      return history::TOUCH_PRECOMPOSED_ICON;
    case FaviconURL::INVALID_ICON:
      return history::INVALID_ICON;
  }
  NOTREACHED();
  // Shouldn't reach here, just make compiler happy.
  return history::INVALID_ICON;
}

bool DoUrlAndIconMatch(const FaviconURL& favicon_url,
                       const GURL& url,
                       history::IconType icon_type) {
  return favicon_url.icon_url == url &&
      favicon_url.icon_type == static_cast<FaviconURL::IconType>(icon_type);
}

// Returns true if all of the icon URLs and icon types in |bitmap_results| are
// identical and if they match the icon URL and icon type in |favicon_url|.
// Returns false if |bitmap_results| is empty.
bool DoUrlsAndIconsMatch(
    const FaviconURL& favicon_url,
    const std::vector<history::FaviconBitmapResult>& bitmap_results) {
  if (bitmap_results.empty())
    return false;

  history::IconType icon_type = ToHistoryIconType(favicon_url.icon_type);

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
bool IsExpired(const history::FaviconBitmapResult& bitmap_result) {
  return bitmap_result.expired;
}

// Return true if |bitmap_result| is valid.
bool IsValid(const history::FaviconBitmapResult& bitmap_result) {
  return bitmap_result.is_valid();
}

// Return list with the current value and historical values of
// history::GetDefaultFaviconSizes().
const std::vector<history::FaviconSizes>& GetHistoricalDefaultFaviconSizes() {
  CR_DEFINE_STATIC_LOCAL(
      std::vector<history::FaviconSizes>, kHistoricalFaviconSizes, ());
  if (kHistoricalFaviconSizes.empty()) {
    kHistoricalFaviconSizes.push_back(history::GetDefaultFaviconSizes());
    // The default favicon sizes used to be a single empty size.
    history::FaviconSizes old_favicon_sizes;
    old_favicon_sizes.push_back(gfx::Size());
    kHistoricalFaviconSizes.push_back(old_favicon_sizes);
  }
  return kHistoricalFaviconSizes;
}

// Returns true if at least one of the bitmaps in |bitmap_results| is expired or
// if |bitmap_results| is known to be incomplete.
bool HasExpiredOrIncompleteResult(
    const std::vector<history::FaviconBitmapResult>& bitmap_results,
    const history::IconURLSizesMap& icon_url_sizes) {
  // Check if at least one of the bitmaps is expired.
  std::vector<history::FaviconBitmapResult>::const_iterator it =
      std::find_if(bitmap_results.begin(), bitmap_results.end(), IsExpired);
  if (it != bitmap_results.end())
    return true;

  // |bitmap_results| is known to be incomplete if the favicon sizes in
  // |icon_url_sizes| for any of the icon URLs in |bitmap_results| are unknown.
  // The favicon sizes for an icon URL are unknown if MergeFavicon() has set
  // them to the default favicon sizes.

  std::set<GURL> icon_urls;
  for (size_t i = 0; i < bitmap_results.size(); ++i)
    icon_urls.insert(bitmap_results[i].icon_url);

  for (std::set<GURL>::iterator it = icon_urls.begin(); it != icon_urls.end();
       ++it) {
    const GURL& icon_url = *it;
    history::IconURLSizesMap::const_iterator icon_url_sizes_it =
        icon_url_sizes.find(icon_url);
    if (icon_url_sizes_it == icon_url_sizes.end()) {
      // |icon_url_sizes| should have an entry for each icon URL in
      // |bitmap_results|.
      NOTREACHED();
      return true;
    }

    const history::FaviconSizes& sizes = icon_url_sizes_it->second;
    const std::vector<history::FaviconSizes>& historical_sizes =
        GetHistoricalDefaultFaviconSizes();
    std::vector<history::FaviconSizes>::const_iterator historical_it =
        std::find(historical_sizes.begin(), historical_sizes.end(), sizes);
    if (historical_it != historical_sizes.end())
      return true;
  }
  return false;
}

// Returns true if at least one of |bitmap_results| is valid.
bool HasValidResult(
    const std::vector<history::FaviconBitmapResult>& bitmap_results) {
  std::vector<history::FaviconBitmapResult>::const_iterator it =
      std::find_if(bitmap_results.begin(), bitmap_results.end(), IsValid);
  return it != bitmap_results.end();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::DownloadRequest::DownloadRequest()
    : icon_type(history::INVALID_ICON) {
}

FaviconHandler::DownloadRequest::~DownloadRequest() {
}

FaviconHandler::DownloadRequest::DownloadRequest(
    const GURL& url,
    const GURL& image_url,
    history::IconType icon_type)
    : url(url),
      image_url(image_url),
      icon_type(icon_type) {
}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconCandidate::FaviconCandidate()
    : score(0),
      icon_type(history::INVALID_ICON) {
}

FaviconHandler::FaviconCandidate::~FaviconCandidate() {
}

FaviconHandler::FaviconCandidate::FaviconCandidate(
    const GURL& url,
    const GURL& image_url,
    const gfx::Image& image,
    float score,
    history::IconType icon_type)
    : url(url),
      image_url(image_url),
      image(image),
      score(score),
      icon_type(icon_type) {
}

////////////////////////////////////////////////////////////////////////////////

FaviconHandler::FaviconHandler(Profile* profile,
                               FaviconHandlerDelegate* delegate,
                               Type icon_type)
    : got_favicon_from_history_(false),
      favicon_expired_or_incomplete_(false),
      icon_types_(icon_type == FAVICON ? history::FAVICON :
          history::TOUCH_ICON | history::TOUCH_PRECOMPOSED_ICON),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);
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
  if (GetFaviconService()) {
    GetFaviconForURL(
        url_,
        icon_types_,
        base::Bind(&FaviconHandler::OnFaviconDataForInitialURL,
                   base::Unretained(this)),
        &cancelable_task_tracker_);
  }
}

FaviconService* FaviconHandler::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
}

bool FaviconHandler::UpdateFaviconCandidate(const GURL& url,
                                            const GURL& image_url,
                                            const gfx::Image& image,
                                            float score,
                                            history::IconType icon_type) {
  bool update_candidate = false;
  bool exact_match = score == 1;
  if (preferred_icon_size() == 0) {
    // No preferred size, use this icon.
    update_candidate = true;
    exact_match = true;
  } else if (favicon_candidate_.icon_type == history::INVALID_ICON) {
    // No current candidate, use this.
    update_candidate = true;
  } else {
    if (exact_match) {
      // Exact match, use this.
      update_candidate = true;
    } else {
      // Compare against current candidate.
      if (score > favicon_candidate_.score)
        update_candidate = true;
    }
  }
  if (update_candidate) {
    favicon_candidate_ = FaviconCandidate(
        url, image_url, image, score, icon_type);
  }
  return exact_match;
}

void FaviconHandler::SetFavicon(
    const GURL& url,
    const GURL& icon_url,
    const gfx::Image& image,
    history::IconType icon_type) {
  if (GetFaviconService() && ShouldSaveFavicon(url))
    SetHistoryFavicons(url, icon_url, icon_type, image);

  if (UrlMatches(url, url_) && icon_type == history::FAVICON) {
    NavigationEntry* entry = GetEntry();
    if (entry) {
      entry->GetFavicon().url = icon_url;
      UpdateFavicon(entry, &image);
    }
  }
}

void FaviconHandler::UpdateFavicon(NavigationEntry* entry,
    const std::vector<history::FaviconBitmapResult>& favicon_bitmap_results) {
  gfx::Image resized_image = FaviconUtil::SelectFaviconFramesFromPNGs(
      favicon_bitmap_results,
      FaviconUtil::GetFaviconScaleFactors(),
      preferred_icon_size());
  if (!resized_image.IsEmpty())
    UpdateFavicon(entry, &resized_image);
}

void FaviconHandler::UpdateFavicon(NavigationEntry* entry,
                                   const gfx::Image* image) {
  // No matter what happens, we need to mark the favicon as being set.
  entry->GetFavicon().valid = true;

  if (!image)
    return;

  entry->GetFavicon().image = *image;
  delegate_->NotifyFaviconUpdated();
}

void FaviconHandler::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& candidates) {

  image_urls_.clear();
  favicon_candidate_ = FaviconCandidate();
  for (std::vector<FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_empty() && (i->icon_type & icon_types_))
      image_urls_.push_back(*i);
  }

  // TODO(davemoore) Should clear on empty url. Currently we ignore it.
  // This appears to be what FF does as well.
  if (image_urls_.empty())
    return;

  if (!GetFaviconService())
    return;

  ProcessCurrentUrl();
}

void FaviconHandler::ProcessCurrentUrl() {
  DCHECK(!image_urls_.empty());

  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  // For FAVICON.
  if (current_candidate()->icon_type == FaviconURL::FAVICON) {
    if (!favicon_expired_or_incomplete_ && entry->GetFavicon().valid &&
        DoUrlAndIconMatch(*current_candidate(), entry->GetFavicon().url,
                          history::FAVICON))
      return;

    entry->GetFavicon().url = current_candidate()->icon_url;
  } else if (!favicon_expired_or_incomplete_ && got_favicon_from_history_ &&
             HasValidResult(history_results_) &&
             DoUrlsAndIconsMatch(*current_candidate(), history_results_)) {
    return;
  }

  if (got_favicon_from_history_)
    DownloadFaviconOrAskHistory(entry->GetURL(), current_candidate()->icon_url,
        ToHistoryIconType(current_candidate()->icon_type));
}

void FaviconHandler::OnDidDownloadFavicon(
    int id,
    const GURL& image_url,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  DownloadRequests::iterator i = download_requests_.find(id);
  if (i == download_requests_.end()) {
    // Currently WebContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  if (current_candidate() &&
      DoUrlAndIconMatch(*current_candidate(), image_url, i->second.icon_type)) {
    float score = 0.0f;
    std::vector<ui::ScaleFactor> scale_factors =
        FaviconUtil::GetFaviconScaleFactors();
    gfx::Image image(SelectFaviconFrames(bitmaps, scale_factors, requested_size,
                                         &score));

    // The downloaded icon is still valid when there is no FaviconURL update
    // during the downloading.
    bool request_next_icon = true;
    if (!bitmaps.empty()) {
      request_next_icon = !UpdateFaviconCandidate(
          i->second.url, image_url, image, score, i->second.icon_type);
    }
    if (request_next_icon && GetEntry() && image_urls_.size() > 1) {
      // Remove the first member of image_urls_ and process the remaining.
      image_urls_.pop_front();
      ProcessCurrentUrl();
    } else if (favicon_candidate_.icon_type != history::INVALID_ICON) {
      // No more icons to request, set the favicon from the candidate.
      SetFavicon(favicon_candidate_.url,
                 favicon_candidate_.image_url,
                 favicon_candidate_.image,
                 favicon_candidate_.icon_type);
      // Reset candidate.
      image_urls_.clear();
      favicon_candidate_ = FaviconCandidate();
    }
  }
  download_requests_.erase(i);
}

NavigationEntry* FaviconHandler::GetEntry() {
  NavigationEntry* entry = delegate_->GetActiveEntry();
  if (entry && UrlMatches(entry->GetURL(), url_))
    return entry;

  // If the URL has changed out from under us (as will happen with redirects)
  // return NULL.
  return NULL;
}

int FaviconHandler::DownloadFavicon(const GURL& image_url, int image_size) {
  if (!image_url.is_valid()) {
    NOTREACHED();
    return 0;
  }
  int id = delegate_->StartDownload(image_url, image_size);
  return id;
}

void FaviconHandler::UpdateFaviconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  // TODO(pkotwicz): pass in all of |image_urls_| to
  // UpdateFaviconMappingsAndFetch().
  std::vector<GURL> icon_urls;
  icon_urls.push_back(icon_url);
  GetFaviconService()->UpdateFaviconMappingsAndFetch(
      page_url, icon_urls, icon_type, preferred_icon_size(), callback, tracker);
}

void FaviconHandler::GetFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  GetFaviconService()->GetFavicon(
      icon_url, icon_type, preferred_icon_size(), callback, tracker);
}

void FaviconHandler::GetFaviconForURL(
    const GURL& page_url,
    int icon_types,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  GetFaviconService()->GetFaviconForURL(
      FaviconService::FaviconForURLParams(
          profile_, page_url, icon_types, preferred_icon_size()),
      callback,
      tracker);
}

void FaviconHandler::SetHistoryFavicons(const GURL& page_url,
                                        const GURL& icon_url,
                                        history::IconType icon_type,
                                        const gfx::Image& image) {
  GetFaviconService()->SetFavicons(page_url, icon_url, icon_type, image);
}

bool FaviconHandler::ShouldSaveFavicon(const GURL& url) {
  if (!profile_->IsOffTheRecord())
    return true;

  // Otherwise store the favicon if the page is bookmarked.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(profile_);
  return bookmark_model && bookmark_model->IsBookmarked(url);
}

void FaviconHandler::OnFaviconDataForInitialURL(
    const std::vector<history::FaviconBitmapResult>& favicon_bitmap_results,
    const history::IconURLSizesMap& icon_url_sizes) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_favicon_from_history_ = true;
  history_results_ = favicon_bitmap_results;

  bool has_results = !favicon_bitmap_results.empty();
  favicon_expired_or_incomplete_ = has_results && HasExpiredOrIncompleteResult(
      favicon_bitmap_results, icon_url_sizes);

  if (has_results && icon_types_ == history::FAVICON &&
      !entry->GetFavicon().valid &&
      (!current_candidate() ||
       DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results))) {
    // The db knows the favicon (although it may be out of date) and the entry
    // doesn't have an icon. Set the favicon now, and if the favicon turns out
    // to be expired (or the wrong url) we'll fetch later on. This way the
    // user doesn't see a flash of the default favicon.

    // The history service sends back results for a single icon URL, so it does
    // not matter which result we get the |icon_url| from.
    entry->GetFavicon().url = favicon_bitmap_results[0].icon_url;
    if (HasValidResult(favicon_bitmap_results))
      UpdateFavicon(entry, favicon_bitmap_results);
    entry->GetFavicon().valid = true;
  }

  if (has_results && !favicon_expired_or_incomplete_) {
    if (current_candidate() &&
        !DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results)) {
      // Mapping in the database is wrong. DownloadFavIconOrAskHistory will
      // update the mapping for this url and download the favicon if we don't
      // already have it.
      DownloadFaviconOrAskHistory(entry->GetURL(),
          current_candidate()->icon_url,
          static_cast<history::IconType>(current_candidate()->icon_type));
    }
  } else if (current_candidate()) {
    // We know the official url for the favicon, by either don't have the
    // favicon or its expired. Continue on to DownloadFaviconOrAskHistory to
    // either download or check history again.
    DownloadFaviconOrAskHistory(entry->GetURL(), current_candidate()->icon_url,
        ToHistoryIconType(current_candidate()->icon_type));
  }
  // else we haven't got the icon url. When we get it we'll ask the
  // renderer to download the icon.
}

void FaviconHandler::DownloadFaviconOrAskHistory(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type) {
  if (favicon_expired_or_incomplete_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(page_url, icon_url, preferred_icon_size(), icon_type);
  } else if (GetFaviconService()) {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (profile_->IsOffTheRecord()) {
      GetFavicon(
          icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      // Issue the request and associate the current page ID with it.
      UpdateFaviconMappingAndFetch(
          page_url, icon_url, icon_type,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)),
          &cancelable_task_tracker_);
    }
  }
}

void FaviconHandler::OnFaviconData(
    const std::vector<history::FaviconBitmapResult>& favicon_bitmap_results,
    const history::IconURLSizesMap& icon_url_sizes) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  bool has_results = !favicon_bitmap_results.empty();
  bool has_expired_or_incomplete_result = HasExpiredOrIncompleteResult(
      favicon_bitmap_results, icon_url_sizes);

  // No need to update the favicon url. By the time we get here
  // UpdateFaviconURL will have set the favicon url.
  if (has_results && icon_types_ == history::FAVICON) {
    if (HasValidResult(favicon_bitmap_results)) {
      // There is a favicon, set it now. If expired we'll download the current
      // one again, but at least the user will get some icon instead of the
      // default and most likely the current one is fine anyway.
      UpdateFavicon(entry, favicon_bitmap_results);
    }
    if (has_expired_or_incomplete_result) {
      // The favicon is out of date. Request the current one.
      ScheduleDownload(entry->GetURL(), entry->GetFavicon().url,
                       preferred_icon_size(),
                       history::FAVICON);
    }
  } else if (current_candidate() &&
      (!has_results || has_expired_or_incomplete_result ||
       !(DoUrlsAndIconsMatch(*current_candidate(), favicon_bitmap_results)))) {
    // We don't know the favicon, it is out of date or its type is not same as
    // one got from page. Request the current one.
    ScheduleDownload(entry->GetURL(), current_candidate()->icon_url,
        preferred_icon_size(),
        ToHistoryIconType(current_candidate()->icon_type));
  }
  history_results_ = favicon_bitmap_results;
}

int FaviconHandler::ScheduleDownload(
    const GURL& url,
    const GURL& image_url,
    int image_size,
    history::IconType icon_type) {
  const int download_id = DownloadFavicon(image_url, image_size);
  if (download_id) {
    // Download ids should be unique.
    DCHECK(download_requests_.find(download_id) == download_requests_.end());
    download_requests_[download_id] =
        DownloadRequest(url, image_url, icon_type);
  }

  return download_id;
}
