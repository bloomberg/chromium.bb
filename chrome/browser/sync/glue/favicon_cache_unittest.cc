// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/favicon_cache.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncFaviconCacheTest : public testing::Test {
 public:
  SyncFaviconCacheTest() : cache_(NULL) {
  }
  virtual ~SyncFaviconCacheTest() {}

  size_t GetFaviconCount() {
    return cache_.NumFaviconsForTest();
  }
  bool ExpectFaviconEquals(std::string page_url, std::string bytes) {
    GURL gurl(page_url);
    scoped_refptr<base::RefCountedMemory> favicon;
    if (!cache_.GetSyncedFaviconForPageURL(gurl, &favicon))
      return false;
    if (favicon->size() != bytes.size())
      return false;
    for (size_t i = 0; i < favicon->size(); ++i) {
      if (bytes[i] != *(favicon->front() + i))
        return false;
    }
    return true;
  }
  FaviconCache* cache() { return &cache_; }

 private:
  FaviconCache cache_;
};

// A freshly constructed cache should be empty.
TEST_F(SyncFaviconCacheTest, Empty) {
  EXPECT_EQ(0U, GetFaviconCount());
}

TEST_F(SyncFaviconCacheTest, ReceiveSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
}

TEST_F(SyncFaviconCacheTest, ReceiveEmptySyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), std::string());
  EXPECT_EQ(0U, GetFaviconCount());
  EXPECT_FALSE(ExpectFaviconEquals(page_url, std::string()));

  // Then receive the actual favicon.
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
}

TEST_F(SyncFaviconCacheTest, ReceiveUpdatedSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  std::string bytes2 = "bytes2";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // The cache should not update existing favicons from tab sync favicons
  // (which can be reassociated several times).
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes2);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
  EXPECT_FALSE(ExpectFaviconEquals(page_url, bytes2));
}

TEST_F(SyncFaviconCacheTest, MultipleMappings) {
  std::string page_url = "http://www.google.com";
  std::string page2_url = "http://bla.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  cache()->OnReceivedSyncFavicon(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // Map another page to the same favicon. They should share the same data.
  cache()->OnReceivedSyncFavicon(GURL(page2_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page2_url, bytes));
}

}  // namespace browser_sync
