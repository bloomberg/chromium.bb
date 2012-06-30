// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_cache_metadata.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

class GDataCacheMetadataMapTest : public testing::Test {
 public:
  GDataCacheMetadataMapTest() {}

  virtual void SetUp() OVERRIDE {
    // Create cache directories.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    cache_paths_ = GDataCache::GetCachePaths(temp_dir_.path());
    ASSERT_TRUE(GDataCache::CreateCacheDirectories(cache_paths_));

    persistent_directory_ = cache_paths_[GDataCache::CACHE_TYPE_PERSISTENT];
    tmp_directory_ = cache_paths_[GDataCache::CACHE_TYPE_TMP];
    pinned_directory_ = cache_paths_[GDataCache::CACHE_TYPE_PINNED];
    outgoing_directory_ = cache_paths_[GDataCache::CACHE_TYPE_OUTGOING];
  }

  virtual void TearDown() OVERRIDE {
    metadata_.reset();
  }

  // Sets up the GDataCacheMetadata object.
  void SetUpCacheMetadata() {
    metadata_.reset(new GDataCacheMetadataMap(
        NULL, base::SequencedWorkerPool::SequenceToken()));
    metadata_->Initialize(cache_paths_);
  }

  // Sets up the cache directories with various files, including stale
  // symbolic links etc. Should be called before SetUpCacheMetadata().
  void SetUpCacheWithVariousFiles() {
    // Create some files in persistent directory.
    //
    CreateFile(persistent_directory_.AppendASCII("id_foo.md5foo"));
    CreateFile(persistent_directory_.AppendASCII("id_bar.local"));
    // "id_baz" is dirty but does not have a symlink in outgoing
    // directory. This file should be removed.
    CreateFile(persistent_directory_.AppendASCII("id_baz.local"));
    // "id_bad" is in persistent directory, but does not have a link in
    // pinned directory. This file should be removed.
    CreateFile(persistent_directory_.AppendASCII("id_bad.md5bad"));
    // "id_symlink" is invalid, as symlink is not allowed here. This should
    // be moreved.
    CreateSymbolicLink(FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull),
                       persistent_directory_.AppendASCII("id_symlink"));

    // Create some files in tmp directory.
    //
    CreateFile(tmp_directory_.AppendASCII("id_qux.md5qux"));
    // "id_quux" is invalid as we shouldn't have a dirty file in "tmp".
    CreateFile(tmp_directory_.AppendASCII("id_quux.local"));
    // "id_symlink_tmp" is invalid, as symlink is not allowed here. This
    // should be moreved.
    CreateSymbolicLink(FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull),
                       tmp_directory_.AppendASCII("id_symlink_tmp"));

    // Create symbolic links in pinned directory.
    //
    // "id_foo" is pinned, and present locally.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_foo.md5foo"),
                       pinned_directory_.AppendASCII("id_foo"));
    // "id_corge" is pinned, but not present locally. It's properly pointing
    // to /dev/null.
    CreateSymbolicLink(FilePath::FromUTF8Unsafe(util::kSymLinkToDevNull),
                       pinned_directory_.AppendASCII("id_corge"));
    // "id_dangling" is pointing to a non-existent file. The symlink should
    // be removed.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_dangling.md5foo"),
                       pinned_directory_.AppendASCII("id_dangling"));
    // "id_outside" is pointing to a file outside of persistent
    // directory. The symlink should be removed.
    CreateSymbolicLink(tmp_directory_.AppendASCII("id_qux.md5qux"),
                       pinned_directory_.AppendASCII("id_outside"));
    // "id_not_symlink" is not a symlink. This should be removed.
    CreateFile(pinned_directory_.AppendASCII("id_not_symlink"));

    // Create symbolic links in outgoing directory.
    //
    // "id_bar" is dirty and committed.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_bar.local"),
                       outgoing_directory_.AppendASCII("id_bar"));
    // "id_foo" is not dirty. This symlink should be removed.
    CreateSymbolicLink(persistent_directory_.AppendASCII("id_foo.md5foo"),
                       outgoing_directory_.AppendASCII("id_foo"));
  }

  // Create a file at |file_path|.
  void CreateFile(const FilePath& file_path) {
    const std::string kFoo = "foo";
    ASSERT_TRUE(file_util::WriteFile(file_path, kFoo.data(), kFoo.size()))
        << ": " << file_path.value();
  }

  // Create an symlink to |target| at |symlink|.
  void CreateSymbolicLink(const FilePath& target, const FilePath& symlink) {
    ASSERT_TRUE(file_util::CreateSymbolicLink(target, symlink))
        << ": " << target.value() << ": " << symlink.value();
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

  ScopedTempDir temp_dir_;
  scoped_ptr<GDataCacheMetadataMap> metadata_;
  std::vector<FilePath> cache_paths_;
  FilePath persistent_directory_;
  FilePath tmp_directory_;
  FilePath pinned_directory_;
  FilePath outgoing_directory_;
};

// Test all the methods of GDataCacheMetadataMap except for
// RemoveTemporaryFiles.
TEST_F(GDataCacheMetadataMapTest, CacheTest) {
  SetUpCacheMetadata();

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

TEST_F(GDataCacheMetadataMapTest, Initialization) {
  using file_util::PathExists;
  using file_util::IsLink;
  SetUpCacheWithVariousFiles();

  // Some files are removed during cache initialization. Make sure these
  // exist beforehand.
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_baz.local")));
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_bad.md5bad")));
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_quux.local")));
  EXPECT_TRUE(PathExists(pinned_directory_.AppendASCII("id_not_symlink")));
  EXPECT_TRUE(IsLink(pinned_directory_.AppendASCII("id_dangling")));
  EXPECT_TRUE(IsLink(pinned_directory_.AppendASCII("id_outside")));
  EXPECT_TRUE(IsLink(outgoing_directory_.AppendASCII("id_foo")));
  EXPECT_TRUE(IsLink(persistent_directory_.AppendASCII("id_symlink")));
  EXPECT_TRUE(IsLink(tmp_directory_.AppendASCII("id_symlink_tmp")));

  SetUpCacheMetadata();

  // Check contents in "persistent" directory.
  //
  // "id_foo" is present and pinned.
  scoped_ptr<GDataCache::CacheEntry> cache_entry;
  cache_entry = metadata_->GetCacheEntry("id_foo", "md5foo");
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ("md5foo", cache_entry->md5);
  EXPECT_EQ(GDataCache::CACHE_TYPE_PERSISTENT, cache_entry->sub_dir_type);
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_PINNED,
            cache_entry->cache_state);
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_foo.md5foo")));
  EXPECT_TRUE(PathExists(pinned_directory_.AppendASCII("id_foo")));
  // The invalid symlink in "outgoing" should be removed.
  EXPECT_FALSE(PathExists(outgoing_directory_.AppendASCII("id_foo")));

  // "id_bar" is present and dirty.
  cache_entry = metadata_->GetCacheEntry("id_bar", "");
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ("local", cache_entry->md5);
  EXPECT_EQ(GDataCache::CACHE_TYPE_PERSISTENT, cache_entry->sub_dir_type);
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT | GDataCache::CACHE_STATE_DIRTY,
            cache_entry->cache_state);
  EXPECT_TRUE(PathExists(persistent_directory_.AppendASCII("id_bar.local")));
  EXPECT_TRUE(PathExists(outgoing_directory_.AppendASCII("id_bar")));

  // "id_baz" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_baz", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(PathExists(persistent_directory_.AppendASCII("id_baz.local")));

  // "id_bad" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_bad", "md5bad");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(PathExists(persistent_directory_.AppendASCII("id_bad.md5bad")));

  // "id_symlink" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_symlink", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(PathExists(persistent_directory_.AppendASCII("id_symlink")));

  // Check contents in "tmp" directory.
  //
  // "id_qux" is just present in tmp directory.
  cache_entry = metadata_->GetCacheEntry("id_qux", "md5qux");
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ("md5qux", cache_entry->md5);
  EXPECT_EQ(GDataCache::CACHE_TYPE_TMP, cache_entry->sub_dir_type);
  EXPECT_EQ(GDataCache::CACHE_STATE_PRESENT, cache_entry->cache_state);
  EXPECT_TRUE(PathExists(tmp_directory_.AppendASCII("id_qux.md5qux")));

  // "id_quux" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_quux", "md5qux");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(PathExists(pinned_directory_.AppendASCII("id_quux.local")));

  // "id_symlink_tmp" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_symlink_tmp", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(PathExists(pinned_directory_.AppendASCII("id_symlink_tmp")));

  // Check contents in "pinned" directory.
  //
  // "id_corge" is pinned but not present.
  cache_entry = metadata_->GetCacheEntry("id_corge", "");
  ASSERT_TRUE(cache_entry.get());
  EXPECT_EQ("", cache_entry->md5);
  EXPECT_EQ(GDataCache::CACHE_TYPE_PINNED, cache_entry->sub_dir_type);
  EXPECT_EQ(GDataCache::CACHE_STATE_PINNED, cache_entry->cache_state);
  EXPECT_TRUE(IsLink(pinned_directory_.AppendASCII("id_corge")));

  // "id_dangling" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_dangling", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(IsLink(pinned_directory_.AppendASCII("id_dangling")));

  // "id_outside" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_outside", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(IsLink(pinned_directory_.AppendASCII("id_outside")));

  // "id_not_symlink" should be removed during cache initialization.
  cache_entry = metadata_->GetCacheEntry("id_not_symlink", "");
  EXPECT_FALSE(cache_entry.get());
  EXPECT_FALSE(IsLink(pinned_directory_.AppendASCII("id_not_symlink")));
}

// Test GDataCacheMetadataMap::RemoveTemporaryFiles.
TEST_F(GDataCacheMetadataMapTest, RemoveTemporaryFilesTest) {
  SetUpCacheMetadata();

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
