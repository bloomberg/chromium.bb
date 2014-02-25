// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_APPCACHE_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_APPCACHE_TEST_HELPER_H_

#include <set>

#include "webkit/browser/appcache/appcache_storage.h"

namespace appcache {
class AppCacheService;
}

namespace content {

// Helper class for inserting data into a ChromeAppCacheService and reading it
// back.
class AppCacheTestHelper : public appcache::AppCacheStorage::Delegate {
 public:
  AppCacheTestHelper();
  virtual ~AppCacheTestHelper();
  void AddGroupAndCache(appcache::AppCacheService* appcache_service,
                        const GURL& manifest_url);

  void GetOriginsWithCaches(appcache::AppCacheService* appcache_service,
                            std::set<GURL>* origins);
 private:
  virtual void OnGroupAndNewestCacheStored(
      appcache::AppCacheGroup* group,
      appcache::AppCache* newest_cache,
      bool success,
      bool would_exceed_quota) OVERRIDE;
  void OnGotAppCacheInfo(int rv);

  int group_id_;
  int appcache_id_;
  int response_id_;
  scoped_refptr<appcache::AppCacheInfoCollection> appcache_info_;
  std::set<GURL>* origins_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(AppCacheTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_APPCACHE_TEST_HELPER_H_
