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
  CacheStorageIndexTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(CacheStorageIndexTest);
};

TEST_F(CacheStorageIndexTest, TestDefaultConstructor) {
  CacheStorageIndex index;
  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(0u, index.GetStorageSize());
}

TEST_F(CacheStorageIndexTest, TestSetCacheSize) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031, index.GetStorageSize());

  EXPECT_TRUE(index.SetCacheSize("baz", 2000));
  EXPECT_EQ(2031, index.GetStorageSize());

  EXPECT_FALSE(index.SetCacheSize("baz", 2000));
  EXPECT_EQ(2031, index.GetStorageSize());

  EXPECT_EQ(2000, index.GetCacheSize("baz"));
  EXPECT_EQ(CacheStorage::kSizeUnknown, index.GetCacheSize("<not-present>"));
}

TEST_F(CacheStorageIndexTest, TestDoomCache) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031, index.GetStorageSize());

  index.DoomCache("bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031 - 19, index.GetStorageSize());
  index.RestoreDoomedCache();
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", (it++)->name);
  EXPECT_EQ("bar", (it++)->name);
  EXPECT_EQ("baz", (it++)->name);
  EXPECT_EQ(1031, index.GetStorageSize());

  index.DoomCache("foo");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031 - 12, index.GetStorageSize());
  index.FinalizeDoomedCache();
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031 - 12, index.GetStorageSize());
}

TEST_F(CacheStorageIndexTest, TestDelete) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19));
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031, index.GetStorageSize());

  auto it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("bar", it->name);
  EXPECT_EQ(19u, it->size);
  it++;
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);

  index.Delete("bar");
  EXPECT_EQ(2u, index.num_entries());
  ASSERT_EQ(2u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1012, index.GetStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);

  index.Delete("baz");
  EXPECT_EQ(1u, index.num_entries());
  ASSERT_EQ(1u, index.ordered_cache_metadata().size());
  EXPECT_EQ(12, index.GetStorageSize());

  it = index.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
}

TEST_F(CacheStorageIndexTest, TestInsert) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000));
  EXPECT_EQ(3u, index.num_entries());
  ASSERT_EQ(3u, index.ordered_cache_metadata().size());
  EXPECT_EQ(1031, index.GetStorageSize());
}

TEST_F(CacheStorageIndexTest, TestMoveOperator) {
  CacheStorageIndex index;
  index.Insert(CacheStorageIndex::CacheMetadata("foo", 12));
  index.Insert(CacheStorageIndex::CacheMetadata("bar", 19));
  index.Insert(CacheStorageIndex::CacheMetadata("baz", 1000));

  CacheStorageIndex index2;
  index2 = std::move(index);

  EXPECT_EQ(3u, index2.num_entries());
  EXPECT_EQ(3u, index2.ordered_cache_metadata().size());
  ASSERT_EQ(1031, index2.GetStorageSize());

  EXPECT_EQ(0u, index.num_entries());
  EXPECT_TRUE(index.ordered_cache_metadata().empty());
  EXPECT_EQ(0u, index.GetStorageSize());

  auto it = index2.ordered_cache_metadata().begin();
  EXPECT_EQ("foo", it->name);
  EXPECT_EQ(12u, it->size);
  it++;
  EXPECT_EQ("bar", it->name);
  EXPECT_EQ(19u, it->size);
  it++;
  EXPECT_EQ("baz", it->name);
  EXPECT_EQ(1000u, it->size);

  EXPECT_EQ(3u, index2.num_entries());
  ASSERT_EQ(3u, index2.ordered_cache_metadata().size());
  EXPECT_EQ(1031, index2.GetStorageSize());
}

}  // namespace content
