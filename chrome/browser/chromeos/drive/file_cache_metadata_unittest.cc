// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_cache_metadata.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

class FileCacheMetadataTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    // Create cache directories.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    metadata_.reset(new FileCacheMetadata(NULL));
  }

 protected:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<FileCacheMetadata> metadata_;
};

// Test all the methods of FileCacheMetadata except for
// RemoveTemporaryFiles.
TEST_F(FileCacheMetadataTest, CacheTest) {
  ASSERT_EQ(FileCacheMetadata::INITIALIZE_CREATED,
            metadata_->Initialize(temp_dir_.path().AppendASCII("test.db")));

  // Save an initial entry.
  std::string test_resource_id("test_resource_id");
  std::string test_md5("test_md5");
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_md5);
    new_cache_entry.set_is_present(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Test that the entry can be retrieved.
  FileCacheEntry cache_entry;
  ASSERT_TRUE(metadata_->GetCacheEntry(test_resource_id, &cache_entry));
  EXPECT_EQ(test_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());

  // resource_id doesn't exist.
  EXPECT_FALSE(metadata_->GetCacheEntry("not_found_resource_id", &cache_entry));

  // Update all attributes.
  test_md5 = "test_md5_2";
  {
    FileCacheEntry updated_cache_entry;
    updated_cache_entry.set_md5(test_md5);
    updated_cache_entry.set_is_pinned(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, updated_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(test_resource_id, &cache_entry));
  EXPECT_EQ(test_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_pinned());

  // Test dirty cache.
  test_md5 = "test_md5_3";
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_md5);
    new_cache_entry.set_is_dirty(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(test_resource_id, &cache_entry));
  EXPECT_EQ(test_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_dirty());

  // Remove the entry.
  metadata_->RemoveCacheEntry(test_resource_id);
  EXPECT_FALSE(metadata_->GetCacheEntry(test_resource_id, &cache_entry));

  // Add another one.
  test_resource_id = "test_resource_id_2";
  test_md5 = "test_md5_4";
  {
    FileCacheEntry new_cache_entry;
    new_cache_entry.set_md5(test_md5);
    new_cache_entry.set_is_present(true);
    metadata_->AddOrUpdateCacheEntry(test_resource_id, new_cache_entry);
  }

  // Make sure the values took.
  ASSERT_TRUE(metadata_->GetCacheEntry(test_resource_id, &cache_entry));
  EXPECT_EQ(test_md5, cache_entry.md5());
  EXPECT_TRUE(cache_entry.is_present());
}

TEST_F(FileCacheMetadataTest, Initialize) {
  const base::FilePath db_path = temp_dir_.path().AppendASCII("test.db");

  // Try to open a bogus file.
  ASSERT_TRUE(
      google_apis::test_util::WriteStringToFile(db_path, "Hello world"));
  ASSERT_EQ(FileCacheMetadata::INITIALIZE_CREATED,
            metadata_->Initialize(db_path));

  // Open an existing DB.
  metadata_.reset(new FileCacheMetadata(NULL));
  EXPECT_EQ(FileCacheMetadata::INITIALIZE_OPENED,
            metadata_->Initialize(db_path));

  // Try to open a nonexistent path.
  base::FilePath non_existent_path(FILE_PATH_LITERAL(
      "/somewhere/nonexistent/test.db"));
  metadata_.reset(new FileCacheMetadata(NULL));
  EXPECT_EQ(FileCacheMetadata::INITIALIZE_FAILED,
            metadata_->Initialize(non_existent_path));
}

}  // namespace internal
}  // namespace drive
