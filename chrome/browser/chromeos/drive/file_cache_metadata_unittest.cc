// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache_metadata.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class FileCacheMetadataTest : public testing::Test {
 public:
  FileCacheMetadataTest() {}

  virtual void SetUp() OVERRIDE {
    // Create cache directories.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_paths_ = FileCache::GetCachePaths(temp_dir_.path());
    ASSERT_TRUE(FileCache::CreateCacheDirectories(cache_paths_));

    persistent_directory_ = cache_paths_[FileCache::CACHE_TYPE_PERSISTENT];
    tmp_directory_ = cache_paths_[FileCache::CACHE_TYPE_TMP];
  }

  virtual void TearDown() OVERRIDE {
    metadata_.reset();
  }

  // Sets up the FileCacheMetadata object.
  void SetUpCacheMetadata() {
    metadata_ = FileCacheMetadata::CreateCacheMetadata(NULL);
    ASSERT_TRUE(metadata_->Initialize(cache_paths_));
  }

  // Sets up the cache directories with various files.
  // Should be called before SetUpCacheMetadata().
  void SetUpCacheWithVariousFiles() {
    // Create some files in persistent directory.
    //
    CreateFile(persistent_directory_.AppendASCII("id_foo.md5foo"));
    CreateFile(persistent_directory_.AppendASCII("id_bar.local"));

    // Create some files in tmp directory.
    //
    CreateFile(tmp_directory_.AppendASCII("id_qux.md5qux"));
    // "id_quux" is invalid as we shouldn't have a dirty file in "tmp".
    CreateFile(tmp_directory_.AppendASCII("id_quux.local"));
  }

  // Create a file at |file_path|.
  void CreateFile(const base::FilePath& file_path) {
    const std::string kFoo = "foo";
    ASSERT_TRUE(google_apis::test_util::WriteStringToFile(file_path, kFoo))
        << ": " << file_path.value();
  }

 protected:
  // Helper function to insert an item with key |resource_id| into |cache_map|.
  // |md5| and |cache_state| are used to create the value FileCacheEntry.
  void InsertIntoMap(FileCacheMetadata::CacheMap* cache_map,
                     const std::string& resource_id,
                     const FileCacheEntry& cache_entry) {
    cache_map->insert(std::make_pair(
        resource_id, cache_entry));
  }

  // Adds all entries in |cache_map| to the metadata storage.
  void AddAllMapEntries(const FileCacheMetadata::CacheMap& cache_map) {
    for (FileCacheMetadata::CacheMap::const_iterator iter = cache_map.begin();
         iter != cache_map.end(); ++iter) {
      metadata_->AddOrUpdateCacheEntry(iter->first, iter->second);
    }
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<FileCacheMetadata> metadata_;
  std::vector<base::FilePath> cache_paths_;
  base::FilePath persistent_directory_;
  base::FilePath tmp_directory_;
};

// Test all the methods of FileCacheMetadata except for
// RemoveTemporaryFiles.
TEST_F(FileCacheMetadataTest, CacheTest) {
  SetUpCacheMetadata();

  // Save an initial entry.
  std::string test_resource_id("test_resource_id");
  std::string test_file_md5("test_file_md5");
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_present(true);
    new_cache_entry.set_is_persistent(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Test that the entry can be retrieved.
  FileCacheEntry cache_entry;
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());
  EXPECT_TRUE(cache_entry.is_persistent());

  // Empty md5 should also work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // resource_id doesn't exist.
  EXPECT_FALSE(metadata_->GetCacheEntry(
      "not_found_resource_id", std::string(), &cache_entry));

  // md5 doesn't match.
  EXPECT_FALSE(metadata_->GetCacheEntry(
      test_resource_id, "mismatch_md5", &cache_entry));

  // Update all attributes.
  test_file_md5 = "test_file_md5_2";
  {
    FileCacheEntry updated_cache_entry;
    updated_cache_entry.set_md5(test_file_md5);
    updated_cache_entry.set_is_pinned(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, updated_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_pinned());

  // Empty m5 should work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Test dirty cache.
  test_file_md5 = "test_file_md5_3";
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_dirty(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_dirty());

  // Empty md5 should work.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Mismatched md5 should also work for dirty entries.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, "mismatch_md5", &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());

  // Remove the entry.
  metadata_->RemoveCacheEntry(test_resource_id);
  EXPECT_FALSE(metadata_->GetCacheEntry(
      test_resource_id, std::string(), &cache_entry));

  // Add another one.
  test_resource_id = "test_resource_id_2";
  test_file_md5 = "test_file_md5_4";
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_file_md5);
    new_cache_entry.set_is_present(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(
      test_resource_id, test_file_md5, &cache_entry));
  EXPECT_EQ(test_file_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());
}

TEST_F(FileCacheMetadataTest, CorruptDB) {
  using file_util::PathExists;
  SetUpCacheWithVariousFiles();

  const base::FilePath db_path =
      cache_paths_[FileCache::CACHE_TYPE_META].Append(
          FileCacheMetadata::kCacheMetadataDBPath);

  // Write a bogus file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(db_path, "Hello world"));

  // Some files are removed during cache initialization. Make sure these
  // exist beforehand.
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_quux.local")));

  SetUpCacheMetadata();

  // Check contents in "persistent" directory.
  //
  // "id_foo" is moved to temporary directory.
  FileCacheEntry cache_entry;
  ASSERT_TRUE(metadata_->GetCacheEntry("id_foo", "md5foo", &cache_entry));
  EXPECT_EQ("md5foo", cache_entry.md5());
  EXPECT_FALSE(cache_entry.is_persistent());
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT),
      cache_entry));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_foo.md5foo")));

  // "id_bar" is present and dirty.
  ASSERT_TRUE(metadata_->GetCacheEntry("id_bar", "", &cache_entry));
  EXPECT_EQ("local", cache_entry.md5());
  EXPECT_EQ(FileCache::CACHE_TYPE_PERSISTENT,
            FileCache::GetSubDirectoryType(cache_entry));
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT |
                              test_util::TEST_CACHE_STATE_DIRTY |
                              test_util::TEST_CACHE_STATE_PERSISTENT),
      cache_entry));
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_bar.local")));

  // Check contents in "tmp" directory.
  //
  // "id_qux" is just present in tmp directory.
  ASSERT_TRUE(metadata_->GetCacheEntry("id_qux", "md5qux", &cache_entry));
  EXPECT_EQ("md5qux", cache_entry.md5());
  EXPECT_EQ(FileCache::CACHE_TYPE_TMP,
            FileCache::GetSubDirectoryType(cache_entry));
  EXPECT_TRUE(test_util::CacheStatesEqual(
      test_util::ToCacheEntry(test_util::TEST_CACHE_STATE_PRESENT),
      cache_entry));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_qux.md5qux")));

  // "id_quux" should be removed during cache initialization.
  EXPECT_FALSE(metadata_->GetCacheEntry("id_quux", "md5qux", &cache_entry));
}

// Test FileCacheMetadata::RemoveTemporaryFiles.
TEST_F(FileCacheMetadataTest, RemoveTemporaryFiles) {
  SetUpCacheMetadata();

  FileCacheMetadata::CacheMap cache_map;
  {
    FileCacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    InsertIntoMap(&cache_map, "<resource_id_1>", cache_entry);
  }
  {
    FileCacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    cache_entry.set_is_persistent(true);
    InsertIntoMap(&cache_map, "<resource_id_2>", cache_entry);
  }
  {
    FileCacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    cache_entry.set_is_persistent(true);
    InsertIntoMap(&cache_map, "<resource_id_3>", cache_entry);
  }
  {
    FileCacheEntry cache_entry;
    cache_entry.set_md5("<md5>");
    cache_entry.set_is_present(true);
    InsertIntoMap(&cache_map, "<resource_id_4>", cache_entry);
  }

  AddAllMapEntries(cache_map);
  metadata_->RemoveTemporaryFiles();
  // resource 1 and 4 should be gone, as these are temporary.
  FileCacheEntry cache_entry;
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_1>", "", &cache_entry));
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_2>", "", &cache_entry));
  EXPECT_TRUE(metadata_->GetCacheEntry("<resource_id_3>", "", &cache_entry));
  EXPECT_FALSE(metadata_->GetCacheEntry("<resource_id_4>", "", &cache_entry));
}

// Don't use TEST_F, as we don't want SetUp() and TearDown() for this test.
TEST(FileCacheMetadataExtraTest, CannotOpenDB) {
  // Create nonexistent cache paths, so the initialization fails due to the
  // failure of opening the DB.
  std::vector<base::FilePath> cache_paths =
      FileCache::GetCachePaths(
          base::FilePath::FromUTF8Unsafe("/somewhere/nonexistent"));

  scoped_ptr<FileCacheMetadata> metadata =
      FileCacheMetadata::CreateCacheMetadata(NULL);
  EXPECT_FALSE(metadata->Initialize(cache_paths));
}

}  // namespace internal
}  // namespace drive
