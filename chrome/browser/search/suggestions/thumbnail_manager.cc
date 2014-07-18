// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/thumbnail_manager.h"

#include "base/memory/ref_counted_memory.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/codec/jpeg_codec.h"

using leveldb_proto::ProtoDatabase;

namespace {

// From JPEG-encoded bytes to SkBitmap.
SkBitmap* DecodeThumbnail(const std::vector<unsigned char>& encoded_data) {
  return gfx::JPEGCodec::Decode(&encoded_data[0], encoded_data.size());
}

}  // namespace

namespace suggestions {

ThumbnailManager::ThumbnailManager() : weak_ptr_factory_(this) {}

ThumbnailManager::ThumbnailManager(
    net::URLRequestContextGetter* url_request_context,
    scoped_ptr<ProtoDatabase<ThumbnailData> > database,
    const base::FilePath& database_dir)
    : url_request_context_(url_request_context),
      database_(database.Pass()),
      database_ready_(false),
      weak_ptr_factory_(this) {
  database_->Init(database_dir, base::Bind(&ThumbnailManager::OnDatabaseInit,
                                           weak_ptr_factory_.GetWeakPtr()));
}

ThumbnailManager::~ThumbnailManager() {}

ThumbnailManager::ThumbnailRequest::ThumbnailRequest() : fetcher(NULL) {}

ThumbnailManager::ThumbnailRequest::ThumbnailRequest(chrome::BitmapFetcher* f)
    : fetcher(f) {}

ThumbnailManager::ThumbnailRequest::~ThumbnailRequest() { delete fetcher; }

void ThumbnailManager::Initialize(const SuggestionsProfile& suggestions) {
  thumbnail_url_map_.clear();
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    if (suggestion.has_thumbnail()) {
      thumbnail_url_map_[GURL(suggestion.url())] = GURL(suggestion.thumbnail());
    }
  }
}

void ThumbnailManager::GetImageForURL(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If |url| is not found in |thumbnail_url_map_|, then invoke |callback| with
  // NULL since there is no associated thumbnail for this |url|.
  GURL thumbnail_url;
  if (!GetThumbnailURL(url, &thumbnail_url)) {
    callback.Run(url, NULL);
    return;
  }

  // |database_| can be NULL if something went wrong in initialization.
  if (database_.get() && !database_ready_) {
    // Once database is initialized, it will serve pending requests from either
    // cache or network.
    QueueCacheRequest(url, thumbnail_url, callback);
    return;
  }

  ServeFromCacheOrNetwork(url, thumbnail_url, callback);
}

bool ThumbnailManager::GetThumbnailURL(const GURL& url, GURL* thumbnail_url) {
  std::map<GURL, GURL>::iterator it = thumbnail_url_map_.find(url);
  if (it == thumbnail_url_map_.end()) return false;  // Not found.
  *thumbnail_url = it->second;
  return true;
}

void ThumbnailManager::QueueCacheRequest(
    const GURL& url, const GURL& thumbnail_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // To be served when the database has loaded.
  ThumbnailRequestMap::iterator it = pending_cache_requests_.find(url);
  if (it == pending_cache_requests_.end()) {
    ThumbnailRequest request(NULL);
    request.url = url;
    request.thumbnail_url = thumbnail_url;
    request.callbacks.push_back(callback);
    pending_cache_requests_[url].swap(&request);
  } else {
    // Request already queued for this url.
    it->second.callbacks.push_back(callback);
  }
}

void ThumbnailManager::ServeFromCacheOrNetwork(
    const GURL& url, const GURL& thumbnail_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // If there is a thumbnail available in memory, return it.
  if (!ServeFromCache(url, callback)) {
    StartOrQueueNetworkRequest(url, thumbnail_url, callback);
  }
}

bool ThumbnailManager::ServeFromCache(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  SkBitmap* bitmap = GetBitmapFromCache(url);
  if (bitmap) {
    callback.Run(url, bitmap);
    return true;
  }
  return false;
}

SkBitmap* ThumbnailManager::GetBitmapFromCache(const GURL& url) {
  ThumbnailMap::iterator thumb_iter = thumbnail_map_.find(url.spec());
  if (thumb_iter != thumbnail_map_.end()) {
    return &thumb_iter->second;
  }
  return NULL;
}

void ThumbnailManager::StartOrQueueNetworkRequest(
    const GURL& url, const GURL& thumbnail_url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  // Before starting to fetch the thumbnail. Look for a request in progress for
  // |thumbnail_url|, and queue if appropriate.
  ThumbnailRequestMap::iterator it = pending_net_requests_.find(thumbnail_url);
  if (it == pending_net_requests_.end()) {
    // |thumbnail_url| is not being fetched, so create a request and initiate
    // the fetch.
    ThumbnailRequest request(new chrome::BitmapFetcher(thumbnail_url, this));
    request.url = url;
    request.callbacks.push_back(callback);
    request.fetcher->Start(
        url_request_context_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
    pending_net_requests_[thumbnail_url].swap(&request);
  } else {
    // Request in progress. Register as an interested callback.
    it->second.callbacks.push_back(callback);
  }
}

void ThumbnailManager::OnFetchComplete(const GURL thumbnail_url,
                                       const SkBitmap* bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ThumbnailRequestMap::iterator thumb_iter =
      pending_net_requests_.find(thumbnail_url);
  DCHECK(thumb_iter != pending_net_requests_.end());

  ThumbnailRequest* request = &thumb_iter->second;

  // Here |bitmap| could be NULL or a pointer to a bitmap which is owned by the
  // BitmapFetcher and which ceases to exist after this function. Pass the
  // un-owned pointer to the registered callbacks.
  for (CallbackVector::iterator callback_iter = request->callbacks.begin();
       callback_iter != request->callbacks.end(); ++callback_iter) {
    callback_iter->Run(request->url, bitmap);
  }

  // Save the bitmap to the in-memory model as well as the database, because it
  // is now the freshest representation of the thumbnail.
  // TODO(mathp): Handle null (no thumbnail found), possible deletion in DB.
  if (bitmap) SaveThumbnail(request->url, *bitmap);

  // Erase the completed ThumbnailRequest.
  pending_net_requests_.erase(thumb_iter);
}

void ThumbnailManager::SaveThumbnail(const GURL& url, const SkBitmap& bitmap) {
  // Update the thumbnail map.
  thumbnail_map_.insert(std::make_pair(url.spec(), bitmap));

  if (!database_ready_) return;

  // Attempt to save a JPEG representation to the database. If not successful,
  // the fetched bitmap will still be inserted in the cache, above.
  std::vector<unsigned char> encoded_data;
  if (EncodeThumbnail(bitmap, &encoded_data)) {
    // Save the resulting bitmap to the database.
    ThumbnailData data;
    data.set_url(url.spec());
    data.set_data(std::string(encoded_data.begin(), encoded_data.end()));
    scoped_ptr<ProtoDatabase<ThumbnailData>::KeyEntryVector> entries_to_save(
        new ProtoDatabase<ThumbnailData>::KeyEntryVector());
    scoped_ptr<std::vector<std::string> > keys_to_remove(
        new std::vector<std::string>());
    entries_to_save->push_back(std::make_pair(data.url(), data));
    database_->UpdateEntries(entries_to_save.Pass(), keys_to_remove.Pass(),
                             base::Bind(&ThumbnailManager::OnDatabaseSave,
                                        weak_ptr_factory_.GetWeakPtr()));
  }
}

void ThumbnailManager::OnDatabaseInit(bool success) {
  if (!success) {
    DVLOG(1) << "Thumbnail database init failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_->LoadEntries(base::Bind(&ThumbnailManager::OnDatabaseLoad,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void ThumbnailManager::OnDatabaseLoad(bool success,
                                      scoped_ptr<ThumbnailVector> entries) {
  if (!success) {
    DVLOG(1) << "Thumbnail database load failed.";
    database_.reset();
    ServePendingCacheRequests();
    return;
  }
  database_ready_ = true;

  LoadEntriesInCache(entries.Pass());
  ServePendingCacheRequests();
}

void ThumbnailManager::OnDatabaseSave(bool success) {
  if (!success) {
    DVLOG(1) << "Thumbnail database save failed.";
    database_.reset();
    database_ready_ = false;
  }
}

void ThumbnailManager::LoadEntriesInCache(
    scoped_ptr<ThumbnailVector> entries) {
  for (ThumbnailVector::iterator it = entries->begin(); it != entries->end();
       ++it) {
    std::vector<unsigned char> encoded_data(it->data().begin(),
                                            it->data().end());

    scoped_ptr<SkBitmap> bitmap(DecodeThumbnail(encoded_data));
    if (bitmap.get()) {
      thumbnail_map_.insert(std::make_pair(it->url(), *bitmap));
    }
  }
}

void ThumbnailManager::ServePendingCacheRequests() {
  for (ThumbnailRequestMap::iterator it = pending_cache_requests_.begin();
       it != pending_cache_requests_.end(); ++it) {
    const ThumbnailRequest& request = it->second;
    for (CallbackVector::const_iterator callback_it = request.callbacks.begin();
         callback_it != request.callbacks.end(); ++callback_it) {
      ServeFromCacheOrNetwork(request.url, request.thumbnail_url, *callback_it);
    }
  }
}

// static
bool ThumbnailManager::EncodeThumbnail(const SkBitmap& bitmap,
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
