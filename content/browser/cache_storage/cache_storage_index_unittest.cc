// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_index.h"

#include <list>
#include <utility>
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class CacheStorageIndexTest : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {}
  CacheStorageIndexTest() : padding_key_("abcd1234") {}

  const std::string padding_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageIndexTest);
};

TEST_F(CacheStorageIndexTest, TestDefaultConstructor) {
  CacheStorageIndex index;
  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(0u, index.GetPaddedStorageSize());
}

TEST_F(CacheStorageIndexTest, TestSetCacheSize) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCacheSize("baz", 2000));
  EXPECT_EQ(12 + 2 + 19 + 3 + 2000 + 4, index.GetPaddedStorageSize());

  EXPECT_FALSE(index.SetCacheSize("baz", 2000));
  EXPECT_EQ(12 + 2 + 19 + 3 + 2000 + 4, index.GetPaddedStorageSize());

  EXPECT_EQ(2000, index.GetCacheSizeForTesting("baz"));
  EXPECT_EQ(CacheStorage::kSizeUnknown,
            index.GetCacheSizeForTesting("<not-present>"));
}

TEST_F(CacheStorageIndexTest, TestSetCachePadding) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  EXPECT_TRUE(index.SetCachePadding("baz", 80));
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 80, index.GetPaddedStorageSize());

  EXPECT_FALSE(index.SetCachePadding("baz", 80));
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 80, index.GetPaddedStorageSize());

  EXPECT_EQ(80, index.GetCachePaddingForTesting("baz"));
  EXPECT_EQ(CacheStorage::kSizeUnknown,
            index.GetCachePaddingForTesting("<not-present>"));
}

TEST_F(CacheStorageIndexTest, TestDoomCache) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  index.DoomCache("bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 1000 + 4, index.GetPaddedStorageSize());
  index.RestoreDoomedCache();
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", (it++)->name);
  EXPECT_EQ("bar", (it++)->name);
  EXPECT_EQ("baz", (it++)->name);
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  index.DoomCache("foo");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(19 + 3 + 1000 + 4, index.GetPaddedStorageSize());
  index.FinalizeDoomedCache();
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(19 + 3 + 1000 + 4, index.GetPaddedStorageSize());
}

TEST_F(CacheStorageIndexTest, TestDelete) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(19 + 2 + 12 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("bar", it->name);
  EXPECT_EQ(19u, it->size);
  EXPECT_EQ(2u, it->padding);
  it++;
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  EXPECT_EQ(3u, it->padding);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);
  EXPECT_EQ(4u, it->padding);

  index.Delete("bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 3 + 1000 + 4, index.GetPaddedStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  EXPECT_EQ(3u, it->padding);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);
  EXPECT_EQ(4u, it->padding);

  index.Delete("baz");
  EXPECT_EQ(1u, index.num_entries());
  ASSERT_EQ(1u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 3, index.GetPaddedStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  EXPECT_EQ(3u, it->padding);
}

TEST_F(CacheStorageIndexTest, TestInsert) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index.GetPaddedStorageSize());
}

TEST_F(CacheStorageIndexTest, TestMoveOperator) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12, 2, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19, 3, padding_key_));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000, 4, padding_key_));

  CacheStorageIndex index2;
  index2 = std::move(index);

  EXPECT_EQ(3u, index2.num_entries());
  EXPECT_EQ(3u, index2.ordered_cache_metadata().size());
  ASSERT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index2.GetPaddedStorageSize());

  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(0u, index.GetPaddedStorageSize());

  auto it = index2.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  EXPECT_EQ(2u, it->padding);
  it++;
  EXPECT_EQ("bar", it->name);
  EXPECT_EQ(19u, it->size);
  EXPECT_EQ(3u, it->padding);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);
  EXPECT_EQ(4u, it->padding);

  EXPECT_EQ(3u, index2.num_entries());
  ASSERT_EQ(3u, index2.ordered_cache_metadata().size());
  EXPECT_EQ(12 + 2 + 19 + 3 + 1000 + 4, index2.GetPaddedStorageSize());
}

}  // namespace content
