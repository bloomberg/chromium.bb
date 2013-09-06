// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore/webstore_cache.h"

#include "base/values.h"

namespace app_list {
namespace {

const int kWebstoreCacheMaxSize = 100;
const int kWebstoreCacheTimeLimitInMinutes = 1;

}  // namespace

void WebstoreCache::CacheDeletor::operator()(WebstoreCache::Payload& payload) {
  delete payload.second;
}

WebstoreCache::WebstoreCache()
    : cache_(kWebstoreCacheMaxSize) {
}

WebstoreCache::~WebstoreCache() {
}

const base::DictionaryValue* WebstoreCache::Get(const std::string& query) {
  Cache::iterator iter = cache_.Get(query);
  if (iter != cache_.end()) {
    if (base::Time::Now() - iter->second.first <=
        base::TimeDelta::FromMinutes(kWebstoreCacheTimeLimitInMinutes)) {
      return iter->second.second;
    } else {
      cache_.Erase(iter);
    }
  }
  return NULL;
}

void WebstoreCache::Put(const std::string& query,
                        scoped_ptr<base::DictionaryValue> result) {
  if (result)
    cache_.Put(query, std::make_pair(base::Time::Now(), result.release()));
}

}  // namespace app_list
