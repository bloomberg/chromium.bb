// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace drive {
namespace internal {

class ResourceMetadataStorageTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(storage_->Initialize());
  }

  // Overwrites |storage_|'s version.
  void SetDBVersion(int version) {
    ResourceMetadataHeader header;
    ASSERT_TRUE(storage_->GetHeader(&header));
    header.set_version(version);
    EXPECT_TRUE(storage_->PutHeader(header));
  }

  bool CheckValidity() {
    return storage_->CheckValidity();
  }

  // Puts a child entry.
  void PutChild(const std::string& parent_id,
                const std::string& child_base_name,
                const std::string& child_id) {
    storage_->resource_map_->Put(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_id, child_base_name),
        child_id);
  }

  // Removes a child entry.
  void RemoveChild(const std::string& parent_id,
                   const std::string& child_base_name) {
    storage_->resource_map_->Delete(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_id, child_base_name));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> storage_;
};

TEST_F(ResourceMetadataStorageTest, LargestChangestamp) {
  const int64 kLargestChangestamp = 1234567890;
  EXPECT_TRUE(storage_->SetLargestChangestamp(kLargestChangestamp));
  EXPECT_EQ(kLargestChangestamp, storage_->GetLargestChangestamp());
}

TEST_F(ResourceMetadataStorageTest, PutEntry) {
  const std::string key1 = "abcdefg";
  const std::string key2 = "abcd";
  const std::string key3 = "efgh";
  const std::string name2 = "ABCD";
  const std::string name3 = "EFGH";

  ResourceEntry entry1;

  // key1 not found.
  ResourceEntry result;
  EXPECT_FALSE(storage_->GetEntry(key1, &result));

  // Put entry1.
  EXPECT_TRUE(storage_->PutEntry(key1, entry1));

  // key1 found.
  EXPECT_TRUE(storage_->GetEntry(key1, &result));

  // key2 not found.
  EXPECT_FALSE(storage_->GetEntry(key2, &result));

  // Put entry2 as a child of entry1.
  ResourceEntry entry2;
  entry2.set_parent_local_id(key1);
  entry2.set_base_name(name2);
  EXPECT_TRUE(storage_->PutEntry(key2, entry2));

  // key2 found.
  EXPECT_TRUE(storage_->GetEntry(key2, &result));
  EXPECT_EQ(key2, storage_->GetChild(key1, name2));

  // Put entry3 as a child of entry2.
  ResourceEntry entry3;
  entry3.set_parent_local_id(key2);
  entry3.set_base_name(name3);
  EXPECT_TRUE(storage_->PutEntry(key3, entry3));

  // key3 found.
  EXPECT_TRUE(storage_->GetEntry(key3, &result));
  EXPECT_EQ(key3, storage_->GetChild(key2, name3));

  // Change entry3's parent to entry1.
  entry3.set_parent_local_id(key1);
  EXPECT_TRUE(storage_->PutEntry(key3, entry3));

  // entry3 is a child of entry1 now.
  EXPECT_TRUE(storage_->GetChild(key2, name3).empty());
  EXPECT_EQ(key3, storage_->GetChild(key1, name3));

  // Remove entries.
  EXPECT_TRUE(storage_->RemoveEntry(key3));
  EXPECT_FALSE(storage_->GetEntry(key3, &result));
  EXPECT_TRUE(storage_->RemoveEntry(key2));
  EXPECT_FALSE(storage_->GetEntry(key2, &result));
  EXPECT_TRUE(storage_->RemoveEntry(key1));
  EXPECT_FALSE(storage_->GetEntry(key1, &result));
}

TEST_F(ResourceMetadataStorageTest, Iterator) {
  // Prepare data.
  std::vector<std::string> keys;

  keys.push_back("entry1");
  keys.push_back("entry2");
  keys.push_back("entry3");
  keys.push_back("entry4");

  for (size_t i = 0; i < keys.size(); ++i)
    EXPECT_TRUE(storage_->PutEntry(keys[i], ResourceEntry()));

  // Insert some cache entries.
  std::map<std::string, FileCacheEntry> cache_entries;
  cache_entries[keys[0]].set_md5("aaaaaa");
  cache_entries[keys[1]].set_md5("bbbbbb");
  for (std::map<std::string, FileCacheEntry>::iterator it =
           cache_entries.begin(); it != cache_entries.end(); ++it)
    EXPECT_TRUE(storage_->PutCacheEntry(it->first, it->second));

  // Iterate and check the result.
  std::map<std::string, ResourceEntry> found_entries;
  std::map<std::string, FileCacheEntry> found_cache_entries;
  scoped_ptr<ResourceMetadataStorage::Iterator> it = storage_->GetIterator();
  ASSERT_TRUE(it);
  for (; !it->IsAtEnd(); it->Advance()) {
    const ResourceEntry& entry = it->GetValue();
    found_entries[it->GetID()] = entry;

    FileCacheEntry cache_entry;
    if (it->GetCacheEntry(&cache_entry))
      found_cache_entries[it->GetID()] = cache_entry;
  }
  EXPECT_FALSE(it->HasError());

  EXPECT_EQ(keys.size(), found_entries.size());
  for (size_t i = 0; i < keys.size(); ++i)
    EXPECT_EQ(1U, found_entries.count(keys[i]));

  EXPECT_EQ(cache_entries.size(), found_cache_entries.size());
  for (std::map<std::string, FileCacheEntry>::iterator it =
           cache_entries.begin(); it != cache_entries.end(); ++it) {
    ASSERT_EQ(1U, found_cache_entries.count(it->first));
    EXPECT_EQ(it->second.md5(), found_cache_entries[it->first].md5());
  }
}

TEST_F(ResourceMetadataStorageTest, PutCacheEntry) {
  FileCacheEntry entry;
  const std::string key1 = "abcdefg";
  const std::string key2 = "abcd";
  const std::string md5_1 = "foo";
  const std::string md5_2 = "bar";

  // Put cache entries.
  entry.set_md5(md5_1);
  EXPECT_TRUE(storage_->PutCacheEntry(key1, entry));
  entry.set_md5(md5_2);
  EXPECT_TRUE(storage_->PutCacheEntry(key2, entry));

  // Get cache entires.
  EXPECT_TRUE(storage_->GetCacheEntry(key1, &entry));
  EXPECT_EQ(md5_1, entry.md5());
  EXPECT_TRUE(storage_->GetCacheEntry(key2, &entry));
  EXPECT_EQ(md5_2, entry.md5());

  // Remove cache entries.
  EXPECT_TRUE(storage_->RemoveCacheEntry(key1));
  EXPECT_FALSE(storage_->GetCacheEntry(key1, &entry));

  EXPECT_TRUE(storage_->RemoveCacheEntry(key2));
  EXPECT_FALSE(storage_->GetCacheEntry(key2, &entry));
}

TEST_F(ResourceMetadataStorageTest, CacheEntryIterator) {
  // Prepare data.
  std::map<std::string, FileCacheEntry> entries;
  FileCacheEntry cache_entry;

  cache_entry.set_md5("aA");
  entries["entry1"] = cache_entry;
  cache_entry.set_md5("bB");
  entries["entry2"] = cache_entry;
  cache_entry.set_md5("cC");
  entries["entry3"] = cache_entry;
  cache_entry.set_md5("dD");
  entries["entry4"] = cache_entry;

  for (std::map<std::string, FileCacheEntry>::iterator it = entries.begin();
       it != entries.end(); ++it)
    EXPECT_TRUE(storage_->PutCacheEntry(it->first, it->second));

  // Insert some dummy entries.
  EXPECT_TRUE(storage_->PutEntry("entry1", ResourceEntry()));
  EXPECT_TRUE(storage_->PutEntry("entry2", ResourceEntry()));

  // Iterate and check the result.
  scoped_ptr<ResourceMetadataStorage::CacheEntryIterator> it =
      storage_->GetCacheEntryIterator();
  ASSERT_TRUE(it);
  size_t num_entries = 0;
  for (; !it->IsAtEnd(); it->Advance()) {
    EXPECT_EQ(1U, entries.count(it->GetID()));
    EXPECT_EQ(entries[it->GetID()].md5(), it->GetValue().md5());
    ++num_entries;
  }
  EXPECT_FALSE(it->HasError());
  EXPECT_EQ(entries.size(), num_entries);
}

TEST_F(ResourceMetadataStorageTest, GetChildren) {
  const std::string parents_id[] = { "mercury", "venus", "mars", "jupiter",
                                     "saturn" };
  std::vector<std::vector<std::pair<std::string, std::string> > >
      children_name_id(arraysize(parents_id));
  // Skip children_name_id[0/1] here because Mercury and Venus have no moon.
  children_name_id[2].push_back(std::make_pair("phobos", "mars_i"));
  children_name_id[2].push_back(std::make_pair("deimos", "mars_ii"));
  children_name_id[3].push_back(std::make_pair("io", "jupiter_i"));
  children_name_id[3].push_back(std::make_pair("europa", "jupiter_ii"));
  children_name_id[3].push_back(std::make_pair("ganymede", "jupiter_iii"));
  children_name_id[3].push_back(std::make_pair("calisto", "jupiter_iv"));
  children_name_id[4].push_back(std::make_pair("mimas", "saturn_i"));
  children_name_id[4].push_back(std::make_pair("enceladus", "saturn_ii"));
  children_name_id[4].push_back(std::make_pair("tethys", "saturn_iii"));
  children_name_id[4].push_back(std::make_pair("dione", "saturn_iv"));
  children_name_id[4].push_back(std::make_pair("rhea", "saturn_v"));
  children_name_id[4].push_back(std::make_pair("titan", "saturn_vi"));
  children_name_id[4].push_back(std::make_pair("iapetus", "saturn_vii"));

  // Put parents.
  for (size_t i = 0; i < arraysize(parents_id); ++i)
    EXPECT_TRUE(storage_->PutEntry(parents_id[i], ResourceEntry()));

  // Put children.
  for (size_t i = 0; i < children_name_id.size(); ++i) {
    for (size_t j = 0; j < children_name_id[i].size(); ++j) {
      ResourceEntry entry;
      entry.set_parent_local_id(parents_id[i]);
      entry.set_base_name(children_name_id[i][j].first);
      EXPECT_TRUE(storage_->PutEntry(children_name_id[i][j].second, entry));
    }
  }

  // Put some dummy cache entries.
  for (size_t i = 0; i < arraysize(parents_id); ++i) {
    FileCacheEntry cache_entry;
    EXPECT_TRUE(storage_->PutCacheEntry(parents_id[i], cache_entry));
  }

  // Try to get children.
  for (size_t i = 0; i < children_name_id.size(); ++i) {
    std::vector<std::string> children;
    storage_->GetChildren(parents_id[i], &children);
    EXPECT_EQ(children_name_id[i].size(), children.size());
    for (size_t j = 0; j < children_name_id[i].size(); ++j) {
      EXPECT_EQ(1, std::count(children.begin(),
                              children.end(),
                              children_name_id[i][j].second));
    }
  }
}

TEST_F(ResourceMetadataStorageTest, OpenExistingDB) {
  const std::string parent_id1 = "abcdefg";
  const std::string child_name1 = "WXYZABC";
  const std::string child_id1 = "qwerty";

  ResourceEntry entry1;
  ResourceEntry entry2;
  entry2.set_parent_local_id(parent_id1);
  entry2.set_base_name(child_name1);

  // Put some data.
  EXPECT_TRUE(storage_->PutEntry(parent_id1, entry1));
  EXPECT_TRUE(storage_->PutEntry(child_id1, entry2));

  // Close DB and reopen.
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.path(), base::MessageLoopProxy::current().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Can read data.
  ResourceEntry result;
  EXPECT_TRUE(storage_->GetEntry(parent_id1, &result));

  EXPECT_TRUE(storage_->GetEntry(child_id1, &result));
  EXPECT_EQ(parent_id1, result.parent_local_id());
  EXPECT_EQ(child_name1, result.base_name());

  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_Old) {
  const int64 kLargestChangestamp = 1234567890;
  const std::string key1 = "abcd";

  // Put some data.
  EXPECT_TRUE(storage_->SetLargestChangestamp(kLargestChangestamp));
  ResourceEntry entry;
  EXPECT_TRUE(storage_->PutEntry(key1, ResourceEntry()));
  EXPECT_TRUE(storage_->GetEntry(key1, &entry));
  FileCacheEntry cache_entry;
  EXPECT_TRUE(storage_->PutCacheEntry(key1, FileCacheEntry()));
  EXPECT_TRUE(storage_->GetCacheEntry(key1, &cache_entry));

  // Set older version and reopen DB.
  SetDBVersion(ResourceMetadataStorage::kDBVersion - 1);
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.path(), base::MessageLoopProxy::current().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased, except cache entries, because of the incompatible version.
  EXPECT_EQ(0, storage_->GetLargestChangestamp());
  EXPECT_FALSE(storage_->GetEntry(key1, &entry));
  EXPECT_TRUE(storage_->GetCacheEntry(key1, &cache_entry));
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB_Unknown) {
  const int64 kLargestChangestamp = 1234567890;
  const std::string key1 = "abcd";

  // Put some data.
  EXPECT_TRUE(storage_->SetLargestChangestamp(kLargestChangestamp));
  ResourceEntry entry;
  EXPECT_TRUE(storage_->PutEntry(key1, ResourceEntry()));
  EXPECT_TRUE(storage_->GetEntry(key1, &entry));
  FileCacheEntry cache_entry;
  EXPECT_TRUE(storage_->PutCacheEntry(key1, FileCacheEntry()));
  EXPECT_TRUE(storage_->GetCacheEntry(key1, &cache_entry));

  // Set newer version and reopen DB.
  SetDBVersion(ResourceMetadataStorage::kDBVersion + 1);
  storage_.reset(new ResourceMetadataStorage(
      temp_dir_.path(), base::MessageLoopProxy::current().get()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased because of the incompatible version.
  EXPECT_EQ(0, storage_->GetLargestChangestamp());
  EXPECT_FALSE(storage_->GetEntry(key1, &entry));
  EXPECT_FALSE(storage_->GetCacheEntry(key1, &cache_entry));
}

TEST_F(ResourceMetadataStorageTest, WrongPath) {
  // Create a file.
  base::FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &path));

  storage_.reset(new ResourceMetadataStorage(
      path, base::MessageLoopProxy::current().get()));
  // Cannot initialize DB beacause the path does not point a directory.
  ASSERT_FALSE(storage_->Initialize());
}

TEST_F(ResourceMetadataStorageTest, CheckValidity) {
  const std::string key1 = "foo";
  const std::string name1 = "hoge";
  const std::string key2 = "bar";
  const std::string name2 = "fuga";
  const std::string key3 = "boo";
  const std::string name3 = "piyo";

  // Empty storage is valid.
  EXPECT_TRUE(CheckValidity());

  // Put entry with key1.
  ResourceEntry entry;
  entry.set_base_name(name1);
  EXPECT_TRUE(storage_->PutEntry(key1, entry));
  EXPECT_TRUE(CheckValidity());

  // Put entry with key2 under key1.
  entry.set_parent_local_id(key1);
  entry.set_base_name(name2);
  EXPECT_TRUE(storage_->PutEntry(key2, entry));
  EXPECT_TRUE(CheckValidity());

  RemoveChild(key1, name2);
  EXPECT_FALSE(CheckValidity());  // Missing parent-child relationship.

  // Add back parent-child relationship between key1 and key2.
  PutChild(key1, name2, key2);
  EXPECT_TRUE(CheckValidity());

  // Add parent-child relationship between key2 and key3.
  PutChild(key2, name3, key3);
  EXPECT_FALSE(CheckValidity());  // key3 is not stored in the storage.

  // Put entry with key3 under key2.
  entry.set_parent_local_id(key2);
  entry.set_base_name(name3);
  EXPECT_TRUE(storage_->PutEntry(key3, entry));
  EXPECT_TRUE(CheckValidity());

  // Parent-child relationship with wrong name.
  RemoveChild(key2, name3);
  EXPECT_FALSE(CheckValidity());
  PutChild(key2, name2, key3);
  EXPECT_FALSE(CheckValidity());

  // Fix up the relationship between key2 and key3.
  RemoveChild(key2, name2);
  EXPECT_FALSE(CheckValidity());
  PutChild(key2, name3, key3);
  EXPECT_TRUE(CheckValidity());

  // Add some cache entries.
  FileCacheEntry cache_entry;
  EXPECT_TRUE(storage_->PutCacheEntry(key1, cache_entry));
  EXPECT_TRUE(storage_->PutCacheEntry(key2, cache_entry));

  // Remove key2.
  RemoveChild(key1, name2);
  EXPECT_FALSE(CheckValidity());
  EXPECT_TRUE(storage_->RemoveEntry(key2));
  EXPECT_FALSE(CheckValidity());

  // Remove key3.
  RemoveChild(key2, name3);
  EXPECT_FALSE(CheckValidity());
  EXPECT_TRUE(storage_->RemoveEntry(key3));
  EXPECT_TRUE(CheckValidity());

  // Remove key1.
  EXPECT_TRUE(storage_->RemoveEntry(key1));
  EXPECT_TRUE(CheckValidity());
}

}  // namespace internal
}  // namespace drive
