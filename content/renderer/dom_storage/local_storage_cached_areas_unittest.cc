// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/local_storage_cached_areas.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/renderer/dom_storage/local_storage_cached_area.h"
#include "content/renderer/dom_storage/mock_leveldb_wrapper.h"
#include "third_party/WebKit/public/platform/scheduler/test/fake_renderer_scheduler.h"

namespace content {

TEST(LocalStorageCachedAreasTest, CacheLimit) {
  base::test::ScopedTaskEnvironment scoped_task_environment;
  const url::Origin kOrigin = url::Origin::Create(GURL("http://dom_storage1/"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://dom_storage2/"));
  const url::Origin kOrigin3 =
      url::Origin::Create(GURL("http://dom_storage3/"));
  const base::string16 kKey = base::ASCIIToUTF16("key");
  const base::string16 kValue = base::ASCIIToUTF16("value");
  const GURL kPageUrl("http://dom_storage/page");
  const std::string kStorageAreaId("7");
  const size_t kCacheLimit = 100;

  blink::scheduler::FakeRendererScheduler renderer_scheduler;

  MockLevelDBWrapper mock_leveldb_wrapper;
  LocalStorageCachedAreas cached_areas(&mock_leveldb_wrapper,
                                       &renderer_scheduler);
  cached_areas.set_cache_limit_for_testing(kCacheLimit);

  scoped_refptr<LocalStorageCachedArea> cached_area1 =
      cached_areas.GetCachedArea(kOrigin);
  cached_area1->SetItem(kKey, kValue, kPageUrl, kStorageAreaId);
  const LocalStorageCachedArea* area1_ptr = cached_area1.get();
  size_t expected_total = (kKey.size() + kValue.size()) * sizeof(base::char16);
  EXPECT_EQ(expected_total, cached_area1->memory_used());
  EXPECT_EQ(expected_total, cached_areas.TotalCacheSize());
  cached_area1 = nullptr;

  scoped_refptr<LocalStorageCachedArea> cached_area2 =
      cached_areas.GetCachedArea(kOrigin2);
  cached_area2->SetItem(kKey, kValue, kPageUrl, kStorageAreaId);
  // Area for kOrigin should still be alive.
  EXPECT_EQ(2 * cached_area2->memory_used(), cached_areas.TotalCacheSize());
  EXPECT_EQ(area1_ptr, cached_areas.GetCachedArea(kOrigin));

  base::string16 long_value(kCacheLimit, 'a');
  cached_area2->SetItem(kKey, long_value, kPageUrl, kStorageAreaId);
  // Cache is cleared when a new area is opened.
  scoped_refptr<LocalStorageCachedArea> cached_area3 =
      cached_areas.GetCachedArea(kOrigin3);
  EXPECT_EQ(cached_area2->memory_used(), cached_areas.TotalCacheSize());
}

}  // namespace content
