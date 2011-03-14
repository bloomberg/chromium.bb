// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon_helper.h"

#include "build/build_config.h"

#include <vector>

#include "base/callback.h"
#include "base/ref_counted_memory.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/render_messages.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

FaviconHelper::FaviconHelper(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      got_favicon_url_(false),
      got_favicon_from_history_(false),
      favicon_expired_(false) {
}

FaviconHelper::~FaviconHelper() {
  SkBitmap empty_image;

  // Call pending download callbacks with error to allow caller to clean up.
  for (DownloadRequests::iterator i = download_requests_.begin();
       i != download_requests_.end(); ++i) {
    if (i->second.callback) {
      i->second.callback->Run(i->first, true, empty_image);
    }
  }
}

void FaviconHelper::FetchFavicon(const GURL& url) {
  cancelable_consumer_.CancelAllRequests();

  url_ = url;

  favicon_expired_ = got_favicon_from_history_ = got_favicon_url_ = false;

  // Request the favicon from the history service. In parallel to this the
  // renderer is going to notify us (well TabContents) when the favicon url is
  // available.
  if (GetFaviconService()) {
    GetFaviconService()->GetFaviconForURL(url_, &cancelable_consumer_,
        NewCallback(this, &FaviconHelper::OnFaviconDataForInitialURL));
  }
}

int FaviconHelper::DownloadImage(const GURL& image_url,
                                 int image_size,
                                 ImageDownloadCallback* callback) {
  DCHECK(callback);  // Must provide a callback.
  return ScheduleDownload(GURL(), image_url, image_size, callback);
}

Profile* FaviconHelper::profile() {
  return tab_contents()->profile();
}

FaviconService* FaviconHelper::GetFaviconService() {
  return profile()->GetFaviconService(Profile::EXPLICIT_ACCESS);
}

void FaviconHelper::SetFavicon(
    const GURL& url,
    const GURL& image_url,
    const SkBitmap& image) {
  const SkBitmap& sized_image =
      (image.width() == kFavIconSize && image.height() == kFavIconSize)
      ? image : ConvertToFaviconSize(image);

  if (GetFaviconService() && ShouldSaveFavicon(url)) {
    std::vector<unsigned char> image_data;
    gfx::PNGCodec::EncodeBGRASkBitmap(sized_image, false, &image_data);
    GetFaviconService()->SetFavicon(url, image_url, image_data);
  }

  if (url == url_) {
    NavigationEntry* entry = GetEntry();
    if (entry)
      UpdateFavicon(entry, sized_image);
  }
}

void FaviconHelper::UpdateFavicon(NavigationEntry* entry,
                                  scoped_refptr<RefCountedMemory> data) {
  SkBitmap image;
  gfx::PNGCodec::Decode(data->front(), data->size(), &image);
  UpdateFavicon(entry, image);
}

void FaviconHelper::UpdateFavicon(NavigationEntry* entry,
                                  const SkBitmap& image) {
  // No matter what happens, we need to mark the favicon as being set.
  entry->favicon().set_is_valid(true);

  if (image.empty())
    return;

  entry->favicon().set_bitmap(image);
  tab_contents()->NotifyNavigationStateChanged(TabContents::INVALIDATE_TAB);
}

void FaviconHelper::OnUpdateFaviconURL(int32 page_id, const GURL& icon_url) {
  // TODO(davemoore) Should clear on empty url. Currently we ignore it.
  // This appears to be what FF does as well.
  if (icon_url.is_empty())
    return;

  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_favicon_url_ = true;

  if (!GetFaviconService())
    return;

  if (!favicon_expired_ && entry->favicon().is_valid() &&
      entry->favicon().url() == icon_url) {
    // We already have the icon, no need to proceed.
    return;
  }

  entry->favicon().set_url(icon_url);

  if (got_favicon_from_history_)
    DownloadFaviconOrAskHistory(entry);
}

bool FaviconHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FaviconHelper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateFavIconURL, OnUpdateFaviconURL)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DidDownloadFavIcon, OnDidDownloadFavicon)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FaviconHelper::OnDidDownloadFavicon(int id,
                                         const GURL& image_url,
                                         bool errored,
                                         const SkBitmap& image) {
  DownloadRequests::iterator i = download_requests_.find(id);
  if (i == download_requests_.end()) {
    // Currently TabContents notifies us of ANY downloads so that it is
    // possible to get here.
    return;
  }

  if (i->second.callback) {
    i->second.callback->Run(id, errored, image);
  } else if (!errored) {
    SetFavicon(i->second.url, image_url, image);
  }

  download_requests_.erase(i);
}

NavigationEntry* FaviconHelper::GetEntry() {
  NavigationEntry* entry = tab_contents()->controller().GetActiveEntry();
  if (entry && entry->url() == url_ &&
      tab_contents()->IsActiveEntry(entry->page_id())) {
    return entry;
  }
  // If the URL has changed out from under us (as will happen with redirects)
  // return NULL.
  return NULL;
}

void FaviconHelper::OnFaviconDataForInitialURL(
    FaviconService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedMemory> data,
    bool expired,
    GURL icon_url) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  got_favicon_from_history_ = true;

  favicon_expired_ = (know_favicon && expired);

  if (know_favicon && !entry->favicon().is_valid() &&
      (!got_favicon_url_ || entry->favicon().url() == icon_url)) {
    // The db knows the favicon (although it may be out of date) and the entry
    // doesn't have an icon. Set the favicon now, and if the favicon turns out
    // to be expired (or the wrong url) we'll fetch later on. This way the
    // user doesn't see a flash of the default favicon.
    entry->favicon().set_url(icon_url);
    if (data.get() && data->size())
      UpdateFavicon(entry, data);
    entry->favicon().set_is_valid(true);
  }

  if (know_favicon && !expired) {
    if (got_favicon_url_ && entry->favicon().url() != icon_url) {
      // Mapping in the database is wrong. DownloadFaviconOrAskHistory will
      // update the mapping for this url and download the favicon if we don't
      // already have it.
      DownloadFaviconOrAskHistory(entry);
    }
  } else if (got_favicon_url_) {
    // We know the official url for the favicon, by either don't have the
    // favicon or its expired. Continue on to DownloadFaviconOrAskHistory to
    // either download or check history again.
    DownloadFaviconOrAskHistory(entry);
  }
  // else we haven't got the icon url. When we get it we'll ask the
  // renderer to download the icon.
}

void FaviconHelper::DownloadFaviconOrAskHistory(NavigationEntry* entry) {
  DCHECK(entry);  // We should only get here if entry is valid.
  if (favicon_expired_) {
    // We have the mapping, but the favicon is out of date. Download it now.
    ScheduleDownload(entry->url(), entry->favicon().url(), kFavIconSize, NULL);
  } else if (GetFaviconService()) {
    // We don't know the favicon, but we may have previously downloaded the
    // favicon for another page that shares the same favicon. Ask for the
    // favicon given the favicon URL.
    if (profile()->IsOffTheRecord()) {
      GetFaviconService()->GetFavicon(
          entry->favicon().url(),
          &cancelable_consumer_,
          NewCallback(this, &FaviconHelper::OnFaviconData));
    } else {
      // Ask the history service for the icon. This does two things:
      // 1. Attempts to fetch the favicon data from the database.
      // 2. If the favicon exists in the database, this updates the database to
      //    include the mapping between the page url and the favicon url.
      // This is asynchronous. The history service will call back when done.
      // Issue the request and associate the current page ID with it.
      GetFaviconService()->UpdateFaviconMappingAndFetch(
          entry->url(),
          entry->favicon().url(), &cancelable_consumer_,
          NewCallback(this, &FaviconHelper::OnFaviconData));
    }
  }
}

void FaviconHelper::OnFaviconData(
    FaviconService::Handle handle,
    bool know_favicon,
    scoped_refptr<RefCountedMemory> data,
    bool expired,
    GURL icon_url) {
  NavigationEntry* entry = GetEntry();
  if (!entry)
    return;

  // No need to update the favicon url. By the time we get here
  // UpdateFavIconURL will have set the favicon url.

  if (know_favicon && data.get() && data->size()) {
    // There is a favicon, set it now. If expired we'll download the current
    // one again, but at least the user will get some icon instead of the
    // default and most likely the current one is fine anyway.
    UpdateFavicon(entry, data);
  }

  if (!know_favicon || expired) {
    // We don't know the favicon, or it is out of date. Request the current one.
    ScheduleDownload(entry->url(), entry->favicon().url(), kFavIconSize, NULL);
  }
}

int FaviconHelper::ScheduleDownload(const GURL& url,
                                    const GURL& image_url,
                                    int image_size,
                                    ImageDownloadCallback* callback) {
  const int download_id = tab_contents()->render_view_host()->DownloadFavIcon(
      image_url, image_size);

  if (download_id) {
    // Download ids should be unique.
    DCHECK(download_requests_.find(download_id) == download_requests_.end());
    download_requests_[download_id] = DownloadRequest(url, image_url, callback);
  }

  return download_id;
}

SkBitmap FaviconHelper::ConvertToFaviconSize(const SkBitmap& image) {
  int width = image.width();
  int height = image.height();
  if (width > 0 && height > 0) {
    calc_favicon_target_size(&width, &height);
    return skia::ImageOperations::Resize(
          image, skia::ImageOperations::RESIZE_LANCZOS3,
          width, height);
  }
  return image;
}

bool FaviconHelper::ShouldSaveFavicon(const GURL& url) {
  if (!profile()->IsOffTheRecord())
    return true;

  // Otherwise store the favicon if the page is bookmarked.
  BookmarkModel* bookmark_model = profile()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(url);
}
