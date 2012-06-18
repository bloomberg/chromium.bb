// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache_metadata.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

class GDataCacheMetadataMapTest : public testing::Test {
 public:
  GDataCacheMetadataMapTest() {}

  virtual void SetUp() OVERRIDE {
    metadata_.reset(new GDataCacheMetadataMap(
        NULL, base::SequencedWorkerPool::SequenceToken()));
    std::vector<FilePath> empty_cache_paths;
    metadata_->Initialize(empty_cache_paths);
  }

  virtual void TearDown() OVERRIDE {
    metadata_.reset();
  }

 protected:
  // Helper function to insert an item with key |resource_id| into |cache_map|.
  // |md5|, |sub_dir_type|, |cache_state| are used to create the value
  // CacheEntry.
  void InsertIntoMap(GDataCacheMetadataMap::CacheMap* cache_map,
                     const std::string& resource_id,
                     const std::string& md5,
                     GDataCache::CacheSubDirectoryType sub_dir_type,
                     int cache_state) {
    cache_map->insert(std::make_pair(
        resource_id, GDataCache::CacheEntry(md5, sub_dir_type, cache_state)));
  }

  scoped_ptr<GDataCacheMetadataMap> metadata_;
};

// Test all the methods of GDataCacheMetadataMap except for
// RemoveTemporaryFiles.
TEST_F(GDataCacheMetadataMapTest, CacheTest) {
  // Save an initial entry.
  std::string test_resource_id("test_resource_id");
  std::string test_file_md5("test_file_md5");
  GDataCache::CacheSubDirectoryType test_sub_dir_type(
      GDataCache::CACHE_TYPE_PERSISTENT);
  int test_cache_state(GDataCache::CACHE_STATE_PRESENT);
  metadata_->UpdateCache(test_resource_id, test_file_md5,
                         test_sub_dir_type, test_cache_state);

  // Test that the entry can be retrieved.
  scoped_ptr<GDataCache::CacheEntry> cache_entry =
      metadata_->GetCacheEntry(test_resource_id, test_file_md5);
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);
  EXPECT_EQ(test_sub_dir_type, cache_entry->sub_dir_type);
  EXPECT_EQ(test_cache_state, cache_entry->cache_state);

  // Empty md5 should also work.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, std::string()).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);

  // resource_id doesn't exist.
  cache_entry = metadata_->GetCacheEntry("not_found_resource_id",
                                         std::string()).Pass();
  EXPECT_FALSE(cache_entry.get());

  // md5 doesn't match.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, "mismatch_md5").Pass();
  EXPECT_FALSE(cache_entry.get());

  // Update all attributes.
  test_file_md5 = "test_file_md5_2";
  test_sub_dir_type = GDataCache::CACHE_TYPE_PINNED;
  test_cache_state = GDataCache::CACHE_STATE_PINNED;
  metadata_->UpdateCache(test_resource_id, test_file_md5, test_sub_dir_type,
                         test_cache_state);

  // Make sure the values took.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, test_file_md5).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);
  EXPECT_EQ(test_sub_dir_type, cache_entry->sub_dir_type);
  EXPECT_EQ(test_cache_state, cache_entry->cache_state);

  // Empty m5 should work.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, std::string()).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);

  // Test dirty cache.
  test_file_md5 = "test_file_md5_3";
  test_sub_dir_type = GDataCache::CACHE_TYPE_TMP;
  test_cache_state = GDataCache::CACHE_STATE_DIRTY;
  metadata_->UpdateCache(test_resource_id, test_file_md5, test_sub_dir_type,
                         test_cache_state);

  // Make sure the values took.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, test_file_md5).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);
  EXPECT_EQ(test_sub_dir_type, cache_entry->sub_dir_type);
  EXPECT_EQ(test_cache_state, cache_entry->cache_state);

  // Empty md5 should work.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, std::string()).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);

  // Mismatched md5 should also work for dirty entries.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, "mismatch_md5").Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);

  // Remove the entry.
  metadata_->RemoveFromCache(test_resource_id);
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, std::string()).Pass();
  EXPECT_FALSE(cache_entry.get());

  // Add another one.
  test_resource_id = "test_resource_id_2";
  test_file_md5 = "test_file_md5_4";
  test_sub_dir_type = GDataCache::CACHE_TYPE_TMP_DOWNLOADS;
  test_cache_state = GDataCache::CACHE_STATE_PRESENT;
  metadata_->UpdateCache(test_resource_id, test_file_md5, test_sub_dir_type,
                         test_cache_state);

  // Make sure the values took.
  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, test_file_md5).Pass();
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ(test_file_md5, cache_entry->md5);
  EXPECT_EQ(test_sub_dir_type, cache_entry->sub_dir_type);
  EXPECT_EQ(test_cache_state, cache_entry->cache_state);

  // Update with CACHE_STATE_NONE should evict the entry.
  test_file_md5 = "test_file_md5_5";
  test_sub_dir_type = GDataCache::CACHE_TYPE_TMP_DOCUMENTS;
  test_cache_state = GDataCache::CACHE_STATE_NONE;
  metadata_->UpdateCache(test_resource_id, test_file_md5, test_sub_dir_type,
                         test_cache_state);

  cache_entry =
      metadata_->GetCacheEntry(test_resource_id, std::string()).Pass();
  EXPECT_FALSE(cache_entry.get());
}

// Test GDataCacheMetadataMap::RemoveTemporaryFiles.
TEST_F(GDataCacheMetadataMapTest, RemoveTemporaryFilesTest) {
  GDataCacheMetadataMap::CacheMap cache_map;
  InsertIntoMap(&cache_map,
                "<resource_id_1>",
                "<md5>",
                GDataCache::CACHE_TYPE_TMP,
                GDataCache::CACHE_STATE_PRESENT);
  InsertIntoMap(&cache_map,
                "<resource_id_2>",
                "<md5>",
                GDataCache::CACHE_TYPE_PINNED,
                GDataCache::CACHE_STATE_PRESENT);
  InsertIntoMap(&cache_map,
                "<resource_id_3>",
                "<md5>",
                GDataCache::CACHE_TYPE_OUTGOING,
                GDataCache::CACHE_STATE_PRESENT);
  InsertIntoMap(&cache_map,
                "<resource_id_4>",
                "<md5>",
                GDataCache::CACHE_TYPE_TMP,
                GDataCache::CACHE_STATE_PRESENT);

  metadata_->cache_map_ = cache_map;
  metadata_->RemoveTemporaryFiles();
  // resource 1 and 4 should be gone, as these are CACHE_TYPE_TMP.
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_1>", "").get());
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_2>", "").get());
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_3>", "").get());
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_4>", "").get());
}

}  // namespace gdata
