// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/appcache_test_helper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_entry.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

AppCacheTestHelper::AppCacheTestHelper()
    : group_id_(0),
      appcache_id_(0),
      response_id_(0),
      origins_(NULL) {}

AppCacheTestHelper::~AppCacheTestHelper() {}

void AppCacheTestHelper::OnGroupAndNewestCacheStored(
    AppCacheGroup* /*group*/,
    AppCache* /*newest_cache*/,
    bool success,
    bool /*would_exceed_quota*/) {
  ASSERT_TRUE(success);
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void AppCacheTestHelper::AddGroupAndCache(AppCacheServiceImpl*
    appcache_service, const GURL& manifest_url) {
  AppCacheGroup* appcache_group =
      new AppCacheGroup(appcache_service->storage(),
                                  manifest_url,
                                  ++group_id_);
  AppCache* appcache = new AppCache(
      appcache_service->storage(), ++appcache_id_);
  AppCacheEntry entry(AppCacheEntry::MANIFEST,
                                ++response_id_);
  appcache->AddEntry(manifest_url, entry);
  appcache->set_complete(true);
  appcache_group->AddCache(appcache);
  appcache_service->storage()->StoreGroupAndNewestCache(appcache_group,
                                                        appcache,
                                                        this);
  // OnGroupAndNewestCacheStored will quit the message loop.
  base::RunLoop().Run();
}

void AppCacheTestHelper::GetOriginsWithCaches(AppCacheServiceImpl*
    appcache_service, std::set<GURL>* origins) {
  appcache_info_ = new AppCacheInfoCollection;
  origins_ = origins;
  appcache_service->GetAllAppCacheInfo(
      appcache_info_.get(),
      base::Bind(&AppCacheTestHelper::OnGotAppCacheInfo,
                 base::Unretained(this)));

  // OnGotAppCacheInfo will quit the message loop.
  base::RunLoop().Run();
}

void AppCacheTestHelper::OnGotAppCacheInfo(int rv) {
  typedef std::map<GURL, AppCacheInfoVector> InfoByOrigin;

  origins_->clear();
  for (InfoByOrigin::const_iterator origin =
           appcache_info_->infos_by_origin.begin();
       origin != appcache_info_->infos_by_origin.end(); ++origin) {
    origins_->insert(origin->first);
  }
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

}  // namespace content
