// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/image_manager.h"

#include "base/bind.h"
#include "components/suggestions/image_encoder.h"
#include "components/suggestions/image_fetcher.h"

using leveldb_proto::ProtoDatabase;

namespace suggestions {

ImageManager::ImageManager() : weak_ptr_factory_(this) {}

ImageManager::ImageManager(scoped_ptr<ImageFetcher> image_fetcher,
                           scoped_ptr<ProtoDatabase<ImageData> > database,
                           const base::FilePath& database_dir)
    : image_fetcher_(image_fetcher.Pass()),
      database_(database.Pass()),
      database_ready_(false),
      weak_ptr_factory_(this) {
  image_fetcher_->SetImageFetcherDelegate(this);
  database_->Init(database_dir, base::Bind(&ImageManager::OnDatabaseInit,
                                           weak_ptr_factory_.GetWeakPtr()));
}

ImageManager::~ImageManager() {}

ImageManager::ImageCacheRequest::ImageCacheRequest() {}

ImageManager::ImageCacheRequest::~ImageCacheRequest() {}

void ImageManager::Initialize(const SuggestionsProfile& suggestions) {
  image_url_map_.clear();
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    if (suggestion.has_thumbnail()) {
      image_url_map_[GURL(suggestion.url())] = GURL(suggestion.thumbnail());
    }
  }
}

void ImageManager::GetImageForURL(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
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

void ImageManager::OnImageFetched(const GURL& url, const SkBitmap* bitmap) {
  if (bitmap)  // |bitmap| can be nullptr if image fetch was unsuccessful.
    SaveImage(url, *bitmap);
}

bool ImageManager::GetImageURL(const GURL& url, GURL* image_url) {
  DCHECK(image_url);
  std::map<GURL, GURL>::iterator it = image_url_map_.find(url);
  if (it == image_url_map_.end()) return false;  // Not found.
  *image_url = it->second;
  return true;
}

void ImageManager::QueueCacheRequest(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // To be served when the database has loaded.
  ImageCacheRequestMap::iterator it = pending_cache_requests_.find(url);
  if (it == pending_cache_requests_.end()) {
    ImageCacheRequest request;
    request.url = url;
    request.image_url = image_url;
    request.callbacks.push_back(callback);
    pending_cache_requests_[url] = request;
  } else {
    // Request already queued for this url.
    it->second.callbacks.push_back(callback);
  }
}

void ImageManager::ServeFromCacheOrNetwork(
    const GURL& url, const GURL& image_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // If there is a image available in memory, return it.
  if (!ServeFromCache(url, callback)) {
    image_fetcher_->StartOrQueueNetworkRequest(url, image_url, callback);
  }
}

bool ImageManager::ServeFromCache(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  SkBitmap* bitmap = GetBitmapFromCache(url);
  if (bitmap) {
    callback.Run(url, bitmap);
    return true;
  }
  return false;
}

SkBitmap* ImageManager::GetBitmapFromCache(const GURL& url) {
  ImageMap::iterator image_iter = image_map_.find(url.spec());
  if (image_iter != image_map_.end()) {
    return &image_iter->second;
  }
  return NULL;
}

void ImageManager::SaveImage(const GURL& url, const SkBitmap& bitmap) {
  // Update the image map.
  image_map_.insert(std::make_pair(url.spec(), bitmap));

  if (!database_ready_) return;

  // Attempt to save a JPEG representation to the database. If not successful,
  // the fetched bitmap will still be inserted in the cache, above.
  std::vector<unsigned char> encoded_data;
  if (EncodeSkBitmapToJPEG(bitmap, &encoded_data)) {
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
                             base::Bind(&ImageManager::OnDatabaseSave,
                                        weak_ptr_factory_.GetWeakPtr()));
  }
}

void ImageManager::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "Image database init failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_->LoadEntries(base::Bind(&ImageManager::OnDatabaseLoad,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ImageManager::OnDatabaseLoad(bool success,
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

void ImageManager::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "Image database save failed.";
    database_.reset();
    database_ready_ = false;
  }
}

void ImageManager::LoadEntriesInCache(scoped_ptr<ImageDataVector> entries) {
  for (ImageDataVector::iterator it = entries->begin(); it != entries->end();
       ++it) {
    std::vector<unsigned char> encoded_data(it->data().begin(),
                                            it->data().end());

    scoped_ptr<SkBitmap> bitmap(DecodeJPEGToSkBitmap(encoded_data));
    if (bitmap.get()) {
      image_map_.insert(std::make_pair(it->url(), *bitmap));
    }
  }
}

void ImageManager::ServePendingCacheRequests() {
  for (ImageCacheRequestMap::iterator it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end(); ++it) {
    const ImageCacheRequest& request = it->second;
    for (CallbackVector::const_iterator callback_it = request.callbacks.begin();
         callback_it != request.callbacks.end(); ++callback_it) {
      ServeFromCacheOrNetwork(request.url, request.image_url, *callback_it);
    }
  }
}

}  // namespace suggestions
