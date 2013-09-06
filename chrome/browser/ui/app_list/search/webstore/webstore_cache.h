// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_CACHE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_CACHE_H_

#include <utility>

#include "base/basictypes.h"
#include "base/containers/mru_cache.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"

namespace base {
class DictionaryValue;
}

namespace app_list {

// WebstoreCache manages the cache of webstore search results which should
// be valid during an input session. This will reduce unnecessary queries
// for typing backspace or so on.
class WebstoreCache {
 public:
  WebstoreCache();
  ~WebstoreCache();

  // Checks the current cache and returns the value for the |query| if it's
  // valid. Otherwise returns NULL.
  const base::DictionaryValue* Get(const std::string& query);

  // Puts the new result to the query.
  void Put(const std::string& query, scoped_ptr<base::DictionaryValue> result);

 private:
  typedef std::pair<base::Time, base::DictionaryValue*> Payload;
  class CacheDeletor {
   public:
    void operator()(Payload& payload);
  };
  typedef base::MRUCacheBase<std::string, Payload, CacheDeletor> Cache;

  Cache cache_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreCache);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_WEBSTORE_CACHE_H_
