// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/common/webservice_cache.h"

#include "base/values.h"

namespace app_list {
namespace {

const int kWebserviceCacheMaxSize = 100;
const int kWebserviceCacheTimeLimitInMinutes = 1;

}  // namespace

void WebserviceCache::CacheDeletor::operator()(
    WebserviceCache::Payload& payload) {
  delete payload.second;
}

WebserviceCache::WebserviceCache()
    : cache_(kWebserviceCacheMaxSize) {
}

WebserviceCache::~WebserviceCache() {
}

const base::DictionaryValue* WebserviceCache::Get(const std::string& query) {
  Cache::iterator iter = cache_.Get(query);
  if (iter != cache_.end()) {
    if (base::Time::Now() - iter->second.first <=
        base::TimeDelta::FromMinutes(kWebserviceCacheTimeLimitInMinutes)) {
      return iter->second.second;
    } else {
      cache_.Erase(iter);
    }
  }
  return NULL;
}

void WebserviceCache::Put(const std::string& query,
                          scoped_ptr<base::DictionaryValue> result) {
  if (result)
    cache_.Put(query, std::make_pair(base::Time::Now(), result.release()));
}

}  // namespace app_list
