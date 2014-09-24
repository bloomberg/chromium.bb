// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/image_manager_impl.h"

#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/codec/jpeg_codec.h"

using leveldb_proto::ProtoDatabase;

namespace {

// From JPEG-encoded bytes to SkBitmap.
SkBitmap* DecodeImage(const std::vector<unsigned char>& encoded_data) {
  return gfx::JPEGCodec::Decode(&encoded_data[0], encoded_data.size());
}

}  // namespace

namespace suggestions {

ImageManagerImpl::ImageManagerImpl() : weak_ptr_factory_(this) {}

ImageManagerImpl::ImageManagerImpl(
    net::URLRequestContextGetter* url_request_context,
    scoped_ptr<ProtoDatabase<ImageData> > database,
    const base::FilePath& database_dir)
    : url_request_context_(url_request_context),
      database_(database.Pass()),
      database_ready_(false),
      weak_ptr_factory_(this) {
  database_->Init(database_dir, base::Bind(&ImageManagerImpl::OnDatabaseInit,
                                           weak_ptr_factory_.GetWeakPtr()));
}

ImageManagerImpl::~ImageManagerImpl() {}

ImageManagerImpl::ImageRequest::ImageRequest() : fetcher(NULL) {}

ImageManagerImpl::ImageRequest::ImageRequest(chrome::BitmapFetcher* f)
    : fetcher(f) {}

ImageManagerImpl::ImageRequest::~ImageRequest() { delete fetcher; }

void ImageManagerImpl::Initialize(const SuggestionsProfile& suggestions) {
  image_url_map_.clear();
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    if (suggestion.has_thumbnail()) {
      image_url_map_[GURL(suggestion.url())] = GURL(suggestion.thumbnail());
    }
  }
}

void ImageManagerImpl::GetImageForURL(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If |url| is not found in |image_url_map_|, then invoke |callback| with
  // NULL since there is no associated image for this |url|.
  GURL image_url;
  if (!GetImageURL(url, &image_url)) {
    callback.Run(url, NULL);
    return;
  }

  // |database_| can be NULL if something went wrong in initialization.
  if (database_.get() && !database_ready_) {
    // Once database is initialized, it will serve pending requests from either
    // cache or network.
    QueueCacheRequest(url, image_url, callback);
    return;
  }

  ServeFromCacheOrNetwork(url, image_url, callback);
}

bool ImageManagerImpl::GetImageURL(const GURL& url, GURL* image_url) {
  std::map<GURL, GURL>::iterator it = image_url_map_.find(url);
  if (it == image_url_map_.end()) return false;  // Not found.
  *image_url = it->second;
  return true;
}

void ImageManagerImpl::QueueCacheRequest(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // To be served when the database has loaded.
  ImageRequestMap::iterator it = pending_cache_requests_.find(url);
  if (it == pending_cache_requests_.end()) {
    ImageRequest request(NULL);
    request.url = url;
    request.image_url = image_url;
    request.callbacks.push_back(callback);
    pending_cache_requests_[url].swap(&request);
  } else {
    // Request already queued for this url.
    it->second.callbacks.push_back(callback);
  }
}

void ImageManagerImpl::ServeFromCacheOrNetwork(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // If there is a image available in memory, return it.
  if (!ServeFromCache(url, callback)) {
    StartOrQueueNetworkRequest(url, image_url, callback);
  }
}

bool ImageManagerImpl::ServeFromCache(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  SkBitmap* bitmap = GetBitmapFromCache(url);
  if (bitmap) {
    callback.Run(url, bitmap);
    return true;
  }
  return false;
}

SkBitmap* ImageManagerImpl::GetBitmapFromCache(const GURL& url) {
  ImageMap::iterator image_iter = image_map_.find(url.spec());
  if (image_iter != image_map_.end()) {
    return &image_iter->second;
  }
  return NULL;
}

void ImageManagerImpl::StartOrQueueNetworkRequest(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // Before starting to fetch the image. Look for a request in progress for
  // |image_url|, and queue if appropriate.
  ImageRequestMap::iterator it = pending_net_requests_.find(image_url);
  if (it == pending_net_requests_.end()) {
    // |image_url| is not being fetched, so create a request and initiate
    // the fetch.
    ImageRequest request(new chrome::BitmapFetcher(image_url, this));
    request.url = url;
    request.callbacks.push_back(callback);
    request.fetcher->Start(
        url_request_context_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
    pending_net_requests_[image_url].swap(&request);
  } else {
    // Request in progress. Register as an interested callback.
    it->second.callbacks.push_back(callback);
  }
}

void ImageManagerImpl::OnFetchComplete(const GURL image_url,
                                       const SkBitmap* bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ImageRequestMap::iterator image_iter = pending_net_requests_.find(image_url);
  DCHECK(image_iter != pending_net_requests_.end());

  ImageRequest* request = &image_iter->second;

  // Here |bitmap| could be NULL or a pointer to a bitmap which is owned by the
  // BitmapFetcher and which ceases to exist after this function. Pass the
  // un-owned pointer to the registered callbacks.
  for (CallbackVector::iterator callback_iter = request->callbacks.begin();
       callback_iter != request->callbacks.end(); ++callback_iter) {
    callback_iter->Run(request->url, bitmap);
  }

  // Save the bitmap to the in-memory model as well as the database, because it
  // is now the freshest representation of the image.
  // TODO(mathp): Handle null (no image found), possible deletion in DB.
  if (bitmap) SaveImage(request->url, *bitmap);

  // Erase the completed ImageRequest.
  pending_net_requests_.erase(image_iter);
}

void ImageManagerImpl::SaveImage(const GURL& url, const SkBitmap& bitmap) {
  // Update the image map.
  image_map_.insert(std::make_pair(url.spec(), bitmap));

  if (!database_ready_) return;

  // Attempt to save a JPEG representation to the database. If not successful,
  // the fetched bitmap will still be inserted in the cache, above.
  std::vector<unsigned char> encoded_data;
  if (EncodeImage(bitmap, &encoded_data)) {
    // Save the resulting bitmap to the database.
    ImageData data;
    data.set_url(url.spec());
    data.set_data(std::string(encoded_data.begin(), encoded_data.end()));
    scoped_ptr<ProtoDatabase<ImageData>::KeyEntryVector> entries_to_save(
        new ProtoDatabase<ImageData>::KeyEntryVector());
    scoped_ptr<std::vector<std::string> > keys_to_remove(
        new std::vector<std::string>());
    entries_to_save->push_back(std::make_pair(data.url(), data));
    database_->UpdateEntries(entries_to_save.Pass(), keys_to_remove.Pass(),
                             base::Bind(&ImageManagerImpl::OnDatabaseSave,
                                        weak_ptr_factory_.GetWeakPtr()));
  }
}

void ImageManagerImpl::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "Image database init failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_->LoadEntries(base::Bind(&ImageManagerImpl::OnDatabaseLoad,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ImageManagerImpl::OnDatabaseLoad(bool success,
                                      scoped_ptr<ImageDataVector> entries) {
  if (!success) {
    DVLOG(1) << "Image database load failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_ready_ = true;

  LoadEntriesInCache(entries.Pass());
  ServePendingCacheRequests();
}

void ImageManagerImpl::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "Image database save failed.";
    database_.reset();
    database_ready_ = false;
  }
}

void ImageManagerImpl::LoadEntriesInCache(scoped_ptr<ImageDataVector> entries) {
  for (ImageDataVector::iterator it = entries->begin(); it != entries->end();
       ++it) {
    std::vector<unsigned char> encoded_data(it->data().begin(),
                                            it->data().end());

    scoped_ptr<SkBitmap> bitmap(DecodeImage(encoded_data));
    if (bitmap.get()) {
      image_map_.insert(std::make_pair(it->url(), *bitmap));
    }
  }
}

void ImageManagerImpl::ServePendingCacheRequests() {
  for (ImageRequestMap::iterator it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end(); ++it) {
    const ImageRequest& request = it->second;
    for (CallbackVector::const_iterator callback_it = request.callbacks.begin();
         callback_it != request.callbacks.end(); ++callback_it) {
      ServeFromCacheOrNetwork(request.url, request.image_url, *callback_it);
    }
  }
}

// static
bool ImageManagerImpl::EncodeImage(const SkBitmap& bitmap,
                                   std::vector<unsigned char>* dest) {
  SkAutoLockPixels bitmap_lock(bitmap);
  if (!bitmap.readyToDraw() || bitmap.isNull()) {
    return false;
  }
  return gfx::JPEGCodec::Encode(
      reinterpret_cast<unsigned char*>(bitmap.getAddr32(0, 0)),
      gfx::JPEGCodec::FORMAT_SkBitmap, bitmap.width(), bitmap.height(),
      bitmap.width() * bitmap.bytesPerPixel(), 100, dest);
}

}  // namespace suggestions
