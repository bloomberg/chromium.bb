// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/favicon_handler.h"

#include "build/build_config.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/icon_messages.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"

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

}  // namespace

FaviconHandler::DownloadRequest::DownloadRequest()
    : icon_type(history::INVALID_ICON) {
}

FaviconHandler::DownloadRequest::~DownloadRequest() {
}

FaviconHandler::DownloadRequest::DownloadRequest(
    const GURL& url,
    const GURL& image_url,
    const FaviconTabHelper::ImageDownloadCallback& callback,
    history::IconType icon_type)
    : url(url),
      image_url(image_url),
      callback(callback),
      icon_type(icon_type) {
}

FaviconHandler::FaviconHandler(Profile* profile,
                               FaviconHandlerDelegate* delegate,
                               Type icon_type)
    : got_favicon_from_history_(false),
      favicon_expired_(false),
      icon_types_(icon_type == FAVICON ? history::FAVICON :
          history::TOUCH_ICON | history::TOUCH_PRECOMPOSED_ICON),
      current_url_index_(0),
      profile_(profile),
      delegate_(delegate) {
  DCHECK(profile_);
  DCHECK(delegate_);
}

FaviconHandler::~FaviconHandler() {
  // Call pending download callbacks with error to allow caller to clean up.
  for (DownloadRequests::iterator i = download_requests_.begin();
       i != download_requests_.end(); ++i) {
    if (!i->second.callback.is_null()) {
      i->second.callback.Run(i->first, true, SkBitmap());
    }
  }
}

void FaviconHandler::FetchFavicon(const GURL& url) {
  cancelable_consumer_.CancelAllRequests();

  url_ = url;

  favicon_expired_ = got_favicon_from_history_ = false;
  current_url_index_ = 0;
  urls_.clear();

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well TabContents) when the favicon url is
  // available.
  if (GetFaviconService()) {
    GetFaviconForURL(url_, icon_types_, &cancelable_consumer_,
        base::Bind(&FaviconHandler::OnFaviconDataForInitialURL,
                   base::Unretained(this)));
  }
}

int FaviconHandler::DownloadImage(
    const GURL& image_url,
    int image_size,
    history::IconType icon_type,
    const FaviconTabHelper::ImageDownloadCallback& callback) {
  return ScheduleDownload(GURL(), image_url, image_size, icon_type, callback);
}

FaviconService* FaviconHandler::GetFaviconService() {
  return profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
}

void FaviconHandler::SetFavicon(
    const GURL& url,
    const GURL& image_url,
    const gfx::Image& image,
    history::IconType icon_type) {
  const SkBitmap& bitmap = image;
  const gfx::Image& sized_image = (preferred_icon_size() == 0 ||
      (preferred_icon_size() == bitmap.width() &&
       preferred_icon_size() == bitmap.height())) ?
      image : ResizeFaviconIfNeeded(image);

  if (GetFaviconService() && ShouldSaveFavicon(url)) {
    std::vector<unsigned char> image_data;
    if (gfx::PNGEncodedDataFromImage(sized_image, &image_data))
      SetHistoryFavicon(url, image_url, image_data, icon_type);
  }

  if (url == url_ && icon_type == history::FAVICON) {
    NavigationEntry* entry = GetEntry();
    if (entry)
      UpdateFavicon(entry, &sized_image);
  }
}

void FaviconHandler::UpdateFavicon(NavigationEntry* entry,
                                  scoped_refptr<RefCountedMemory> data) {
  scoped_ptr<gfx::Image> image(gfx::ImageFromPNGEncodedData(data->front(),
                                                            data->size()));
  UpdateFavicon(entry, image.get());
}

void FaviconHandler::UpdateFavicon(NavigationEntry* entry,
                                   const gfx::Image* image) {
  // No matter what happens, we need to mark the favicon as being set.
  entry->favicon().set_is_valid(true);

  if (!image)
    return;

  entry->favicon().set_bitmap(*image);
  delegate_->NotifyFaviconUpdated();
}

void FaviconHandler::OnUpdateFaviconURL(
    int32 page_id,
    const std::vector<FaviconURL>& candidates) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  bool got_favicon_url_update = false;
  for (std::vector<FaviconURL>::const_iterator i = candidates.begin();
       i != candidates.end(); ++i) {
    if (!i->icon_url.is_empty() && (i->icon_type & icon_types_)) {
      if (!got_favicon_url_update) {
        got_favicon_url_update = true;
        urls_.clear();
        current_url_index_ = 0;
      }
      urls_.push_back(*i);
    }
  }

  // TODO(davemoore) Should clear on empty url. Currently we ignore it.
  // This appears to be what FF does as well.
  // No URL was added.
  if (!got_favicon_url_update)
    return;

  if (!GetFaviconService())
    return;

  // For FAVICON.
  if (current_candidate()->icon_type == FaviconURL::FAVICON) {
    if (!favicon_expired_ && entry->favicon().is_valid() &&
        DoUrlAndIconMatch(*current_candidate(), entry->favicon().url(),
                          history::FAVICON))
      return;

    entry->favicon().set_url(current_candidate()->icon_url);
  } else if (!favicon_expired_ && got_favicon_from_history_ &&
              history_icon_.is_valid() &&
              DoUrlAndIconMatch(
                  *current_candidate(),
                  history_icon_.icon_url, history_icon_.icon_type)) {
    return;
  }

  if (got_favicon_from_history_)
    DownloadFaviconOrAskHistory(entry->url(), current_candidate()->icon_url,
        ToHistoryIconType(current_candidate()->icon_type));
}

void FaviconHandler::OnDidDownloadFavicon(int id,
                                          const GURL& image_url,
                                          bool errored,
                                          const gfx::Image& image) {
  DownloadRequests::iterator i = download_requests_.find(id);
  if (i == download_requests_.end()) {
    // Currently TabContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  if (!i->second.callback.is_null()) {
    i->second.callback.Run(id, errored, *(&image));
  } else if (current_candidate() &&
             DoUrlAndIconMatch(*current_candidate(), image_url,
                               i->second.icon_type)) {
    // The downloaded icon is still valid when there is no FaviconURL update
    // during the downloading.
    if (!errored) {
      SetFavicon(i->second.url, image_url, image, i->second.icon_type);
    } else if (GetEntry() && ++current_url_index_ < urls_.size()) {
      // Copies all candidate except first one and notifies the FaviconHandler,
      // so the next candidate can be processed.
      std::vector<FaviconURL> new_candidates(++urls_.begin(), urls_.end());
      OnUpdateFaviconURL(0, new_candidates);
    }
  }
  download_requests_.erase(i);
}

NavigationEntry* FaviconHandler::GetEntry() {
  NavigationEntry* entry = delegate_->GetActiveEntry();
  if (entry && entry->url() == url_)
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
  static int next_id = 1;
  int id = next_id++;
  delegate_->StartDownload(id, image_url, image_size);
  return id;
}

void FaviconHandler::UpdateFaviconMappingAndFetch(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type,
    CancelableRequestConsumerBase* consumer,
    const FaviconService::FaviconDataCallback& callback) {
  GetFaviconService()->UpdateFaviconMappingAndFetch(page_url, icon_url,
      icon_type, consumer, callback);
}

void FaviconHandler::GetFavicon(
    const GURL& icon_url,
    history::IconType icon_type,
    CancelableRequestConsumerBase* consumer,
    const FaviconService::FaviconDataCallback& callback) {
  GetFaviconService()->GetFavicon(icon_url, icon_type, consumer, callback);
}

void FaviconHandler::GetFaviconForURL(
    const GURL& page_url,
    int icon_types,
    CancelableRequestConsumerBase* consumer,
    const FaviconService::FaviconDataCallback& callback) {
  GetFaviconService()->GetFaviconForURL(page_url, icon_types, consumer,
                                        callback);
}

void FaviconHandler::SetHistoryFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    const std::vector<unsigned char>& image_data,
    history::IconType icon_type) {
  GetFaviconService()->SetFavicon(page_url, icon_url, image_data, icon_type);
}

bool FaviconHandler::ShouldSaveFavicon(const GURL& url) {
  if (!profile_->IsOffTheRecord())
    return true;

  // Otherwise store the favicon if the page is bookmarked.
  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(url);
}

void FaviconHandler::OnFaviconDataForInitialURL(
    FaviconService::Handle handle,
    history::FaviconData favicon) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_favicon_from_history_ = true;
  history_icon_ = favicon;

  favicon_expired_ = (favicon.known_icon && favicon.expired);

  if (favicon.known_icon && favicon.icon_type == history::FAVICON &&
      !entry->favicon().is_valid() &&
      (!current_candidate() ||
       DoUrlAndIconMatch(
           *current_candidate(), favicon.icon_url, favicon.icon_type))) {
    // The db knows the favicon (although it may be out of date) and the entry
    // doesn't have an icon. Set the favicon now, and if the favicon turns out
    // to be expired (or the wrong url) we'll fetch later on. This way the
    // user doesn't see a flash of the default favicon.
    entry->favicon().set_url(favicon.icon_url);
    if (favicon.is_valid())
      UpdateFavicon(entry, favicon.image_data);
    entry->favicon().set_is_valid(true);
  }

  if (favicon.known_icon && !favicon.expired) {
    if (current_candidate() &&
        !DoUrlAndIconMatch(
             *current_candidate(), favicon.icon_url, favicon.icon_type)) {
      // Mapping in the database is wrong. DownloadFavIconOrAskHistory will
      // update the mapping for this url and download the favicon if we don't
      // already have it.
      DownloadFaviconOrAskHistory(entry->url(), current_candidate()->icon_url,
          static_cast<history::IconType>(current_candidate()->icon_type));
    }
  } else if (current_candidate()) {
    // We know the official url for the favicon, by either don't have the
    // favicon or its expired. Continue on to DownloadFaviconOrAskHistory to
    // either download or check history again.
    DownloadFaviconOrAskHistory(entry->url(), current_candidate()->icon_url,
        ToHistoryIconType(current_candidate()->icon_type));
  }
  // else we haven't got the icon url. When we get it we'll ask the
  // renderer to download the icon.
}

void FaviconHandler::DownloadFaviconOrAskHistory(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type) {
  if (favicon_expired_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(page_url, icon_url, preferred_icon_size(), icon_type,
                     FaviconTabHelper::ImageDownloadCallback());
  } else if (GetFaviconService()) {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (profile_->IsOffTheRecord()) {
      GetFavicon(icon_url, icon_type, &cancelable_consumer_,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)));
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      // Issue the request and associate the current page ID with it.
      UpdateFaviconMappingAndFetch(page_url, icon_url, icon_type,
          &cancelable_consumer_,
          base::Bind(&FaviconHandler::OnFaviconData, base::Unretained(this)));
    }
  }
}

void FaviconHandler::OnFaviconData(FaviconService::Handle handle,
                                  history::FaviconData favicon) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  // No need to update the favicon url. By the time we get here
  // UpdateFaviconURL will have set the favicon url.
  if (favicon.icon_type == history::FAVICON) {
    if (favicon.is_valid()) {
      // There is a favicon, set it now. If expired we'll download the current
      // one again, but at least the user will get some icon instead of the
      // default and most likely the current one is fine anyway.
      UpdateFavicon(entry, favicon.image_data);
    }
    if (!favicon.known_icon || favicon.expired) {
      // We don't know the favicon, or it is out of date. Request the current
      // one.
      ScheduleDownload(entry->url(), entry->favicon().url(),
                       preferred_icon_size(),
                       history::FAVICON,
                       FaviconTabHelper::ImageDownloadCallback());
    }
  } else if (current_candidate() && (!favicon.known_icon || favicon.expired ||
      !(DoUrlAndIconMatch(
            *current_candidate(), favicon.icon_url, favicon.icon_type)))) {
    // We don't know the favicon, it is out of date or its type is not same as
    // one got from page. Request the current one.
    ScheduleDownload(entry->url(), current_candidate()->icon_url,
        preferred_icon_size(),
        ToHistoryIconType(current_candidate()->icon_type),
        FaviconTabHelper::ImageDownloadCallback());
  }
  history_icon_ = favicon;
}

int FaviconHandler::ScheduleDownload(
    const GURL& url,
    const GURL& image_url,
    int image_size,
    history::IconType icon_type,
    const FaviconTabHelper::ImageDownloadCallback& callback) {
  const int download_id = DownloadFavicon(image_url, image_size);
  if (download_id) {
    // Download ids should be unique.
    DCHECK(download_requests_.find(download_id) == download_requests_.end());
    download_requests_[download_id] =
        DownloadRequest(url, image_url, callback, icon_type);
  }

  return download_id;
}

gfx::Image FaviconHandler::ResizeFaviconIfNeeded(const gfx::Image& image) {
  // Get an SkBitmap from the gfx::Image.
  const SkBitmap& bitmap = image;
  int width = bitmap.width();
  int height = bitmap.height();
  if (width > 0 && height > 0) {
    gfx::CalculateFaviconTargetSize(&width, &height);
    return gfx::Image(new SkBitmap(
                          skia::ImageOperations::Resize(
                              bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
                              width, height)));
  }

  return image;
}
