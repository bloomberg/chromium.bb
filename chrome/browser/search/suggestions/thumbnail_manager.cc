// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/thumbnail_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"

namespace suggestions {

ThumbnailManager::ThumbnailManager(Profile* profile)
    : url_request_context_(profile->GetRequestContext()) {}

ThumbnailManager::~ThumbnailManager() {}

ThumbnailManager::ThumbnailRequest::ThumbnailRequest() : fetcher(NULL) {}

ThumbnailManager::ThumbnailRequest::ThumbnailRequest(chrome::BitmapFetcher* f)
    : fetcher(f) {}

ThumbnailManager::ThumbnailRequest::~ThumbnailRequest() {
  delete fetcher;
}

void ThumbnailManager::InitializeThumbnailMap(
    const SuggestionsProfile& suggestions) {
  thumbnail_map_.clear();
  for (int i = 0; i < suggestions.suggestions_size(); ++i) {
    const ChromeSuggestion& suggestion = suggestions.suggestions(i);
    if (suggestion.has_thumbnail()) {
      thumbnail_map_[GURL(suggestion.url())] = GURL(suggestion.thumbnail());
    }
  }
}

void ThumbnailManager::GetPageThumbnail(
    const GURL& url,
    base::Callback<void(const GURL&, const SkBitmap*)> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // If |url| is not found in |thumbnail_map_|, then invoke |callback| with NULL
  // since there is no associated thumbnail.
  GURL thumbnail_url;
  if (!GetThumbnailURL(url, &thumbnail_url)) {
    callback.Run(url, NULL);
    return;
  }

  // Look for a request in progress for |thumbnail_url|.
  ThumbnailRequestMap::iterator it = pending_requests_.find(thumbnail_url);
  if (it == pending_requests_.end()) {
    // |thumbnail_url| is not being fetched, so create a request and initiate
    // the fetch.
    ThumbnailRequest request(new chrome::BitmapFetcher(thumbnail_url, this));
    request.url = url;
    request.callbacks.push_back(callback);
    request.fetcher->Start(
        url_request_context_, std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
    pending_requests_[thumbnail_url].swap(&request);
  } else {
    // Request in progress. Register as an interested callback.
    it->second.callbacks.push_back(callback);
  }
}

void ThumbnailManager::OnFetchComplete(const GURL thumbnail_url,
                                       const SkBitmap* bitmap) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ThumbnailRequestMap::iterator it = pending_requests_.find(thumbnail_url);
  DCHECK(it != pending_requests_.end());

  ThumbnailRequest* request = &it->second;

  // Here |bitmap| could be NULL or a pointer to a bitmap which is owned by the
  // BitmapFetcher and which ceases to exist after this function. Pass the
  // un-owned pointer to the registered callbacks.
  for (CallbackVector::iterator it = request->callbacks.begin();
       it != request->callbacks.end(); ++it) {
    it->Run(request->url, bitmap);
  }
  pending_requests_.erase(it);
}

bool ThumbnailManager::GetThumbnailURL(const GURL& url, GURL* thumbnail_url) {
  std::map<GURL, GURL>::iterator it = thumbnail_map_.find(url);
  if (it == thumbnail_map_.end()) return false;  // Not found.
  *thumbnail_url = it->second;
  return true;
}

}  // namespace suggestions
