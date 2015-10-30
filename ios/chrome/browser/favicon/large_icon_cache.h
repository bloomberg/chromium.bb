// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_LARGE_ICON_CACHE_H_
#define IOS_CHROME_BROWSER_FAVICON_LARGE_ICON_CACHE_H_

#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
struct LargeIconCacheEntry;

namespace favicon_base {
struct LargeIconResult;
}

namespace ios {
class ChromeBrowserState;
}

// Provides a cache of most recently used LargeIconResult.
//
// Example usage:
//   LargeIconCache* large_icon_cache =
//       IOSChromeLargeIconServiceFactory::GetForBrowserState(browser_state);
//   scoped_ptr<favicon_base::LargeIconResult> icon =
//       large_icon_cache->GetCachedResult(...);
//
class LargeIconCache : public KeyedService {
 public:
  LargeIconCache();
  ~LargeIconCache() override;

  // |LargeIconService| does everything on callbacks, and iOS needs to load the
  // icons immediately on page load. This caches the LargeIconResult so we can
  // immediately load.
  void SetCachedResult(const GURL& url, const favicon_base::LargeIconResult&);

  // Returns a cached LargeIconResult.
  scoped_ptr<favicon_base::LargeIconResult> GetCachedResult(const GURL& url);

 private:
  // Clones a LargeIconResult.
  scoped_ptr<favicon_base::LargeIconResult> CloneLargeIconResult(
      const favicon_base::LargeIconResult& large_icon_result);

  base::OwningMRUCache<GURL, LargeIconCacheEntry*> cache_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconCache);
};

#endif  // IOS_CHROME_BROWSER_FAVICON_LARGE_ICON_CACHE_H_
