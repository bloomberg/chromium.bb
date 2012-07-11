// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

TEST(GDataCacheEntryTest, CacheStateChanges) {
  GDataCacheEntry cache_entry("dummy_md5", CACHE_STATE_NONE);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());

  cache_entry.SetPresent(true);
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());

  cache_entry.SetPinned(true);
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());

  cache_entry.SetDirty(true);
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());

  cache_entry.SetMounted(true);
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());

  cache_entry.SetPersistent(true);
  EXPECT_TRUE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetPresent(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetPresent(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_TRUE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetPinned(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_TRUE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetDirty(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_TRUE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetMounted(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_TRUE(cache_entry.IsPersistent());

  cache_entry.SetPersistent(false);
  EXPECT_FALSE(cache_entry.IsPresent());
  EXPECT_FALSE(cache_entry.IsPinned());
  EXPECT_FALSE(cache_entry.IsDirty());
  EXPECT_FALSE(cache_entry.IsMounted());
  EXPECT_FALSE(cache_entry.IsPersistent());
}

}   // namespace gdata
