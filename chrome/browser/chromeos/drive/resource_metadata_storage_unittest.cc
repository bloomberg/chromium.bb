// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/resource_metadata_storage.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace drive {

namespace {

// Stores the entry to the map.
void StoreEntryToMap(std::map<std::string,ResourceEntry>* out,
                     const ResourceEntry& entry) {
  (*out)[entry.resource_id()] = entry;
}

}  // namespace

class ResourceMetadataStorageTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    storage_.reset(new ResourceMetadataStorage(temp_dir_.path()));
    ASSERT_TRUE(storage_->Initialize());
  }

  virtual void TearDown() OVERRIDE {
  }

  // Overwrites |storage_|'s version.
  void SetDBVersion(int version) {
    scoped_ptr<ResourceMetadataHeader> header = storage_->GetHeader();
    ASSERT_TRUE(header);
    header->set_version(version);
    storage_->PutHeader(*header);
  }

  bool CheckValidity() {
    return storage_->CheckValidity();
  }

  // Puts a child entry.
  void PutChild(const std::string& parent_resource_id,
                const std::string& child_base_name,
                const std::string& child_resource_id) {
    storage_->resource_map_->Put(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_resource_id,
                                                  child_base_name),
        child_resource_id);
  }

  // Removes a child entry.
  void RemoveChild(const std::string& parent_resource_id,
                   const std::string& child_base_name) {
    storage_->resource_map_->Delete(
        leveldb::WriteOptions(),
        ResourceMetadataStorage::GetChildEntryKey(parent_resource_id,
                                                  child_base_name));
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<ResourceMetadataStorage> storage_;
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
  entry1.set_resource_id(key1);

  // key1 not found.
  EXPECT_FALSE(storage_->GetEntry(key1));

  // Put entry1.
  EXPECT_TRUE(storage_->PutEntry(entry1));

  // key1 found.
  scoped_ptr<ResourceEntry> result;
  result = storage_->GetEntry(key1);
  ASSERT_TRUE(result);
  EXPECT_EQ(key1, result->resource_id());

  // key2 not found.
  EXPECT_FALSE(storage_->GetEntry(key2));

  // Put entry2 as a child of entry1.
  ResourceEntry entry2;
  entry2.set_parent_resource_id(key1);
  entry2.set_resource_id(key2);
  entry2.set_base_name(name2);
  EXPECT_TRUE(storage_->PutEntry(entry2));

  // key2 found.
  EXPECT_TRUE(storage_->GetEntry(key2));
  EXPECT_EQ(key2, storage_->GetChild(key1, name2));

  // Put entry3 as a child of entry2.
  ResourceEntry entry3;
  entry3.set_parent_resource_id(key2);
  entry3.set_resource_id(key3);
  entry3.set_base_name(name3);
  EXPECT_TRUE(storage_->PutEntry(entry3));

  // key3 found.
  EXPECT_TRUE(storage_->GetEntry(key3));
  EXPECT_EQ(key3, storage_->GetChild(key2, name3));

  // Change entry3's parent to entry1.
  entry3.set_parent_resource_id(key1);
  EXPECT_TRUE(storage_->PutEntry(entry3));

  // entry3 is a child of entry1 now.
  EXPECT_TRUE(storage_->GetChild(key2, name3).empty());
  EXPECT_EQ(key3, storage_->GetChild(key1, name3));

  // Remove entries.
  EXPECT_TRUE(storage_->RemoveEntry(key3));
  EXPECT_FALSE(storage_->GetEntry(key3));
  EXPECT_TRUE(storage_->RemoveEntry(key2));
  EXPECT_FALSE(storage_->GetEntry(key2));
  EXPECT_TRUE(storage_->RemoveEntry(key1));
  EXPECT_FALSE(storage_->GetEntry(key1));
}

TEST_F(ResourceMetadataStorageTest, Iterate) {
  // Prepare data.
  std::vector<ResourceEntry> entries;
  ResourceEntry entry;

  entry.set_resource_id("entry1");
  entries.push_back(entry);
  entry.set_resource_id("entry2");
  entries.push_back(entry);
  entry.set_resource_id("entry3");
  entries.push_back(entry);
  entry.set_resource_id("entry4");
  entries.push_back(entry);

  for (size_t i = 0; i < entries.size(); ++i)
    EXPECT_TRUE(storage_->PutEntry(entries[i]));

  // Call Iterate and check the result.
  std::map<std::string, ResourceEntry> result;
  storage_->Iterate(base::Bind(&StoreEntryToMap, base::Unretained(&result)));

  EXPECT_EQ(entries.size(), result.size());
  for (size_t i = 0; i < entries.size(); ++i)
    EXPECT_EQ(1U, result.count(entries[i].resource_id()));
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
  for (size_t i = 0; i < arraysize(parents_id); ++i) {
    ResourceEntry entry;
    entry.set_resource_id(parents_id[i]);
    EXPECT_TRUE(storage_->PutEntry(entry));
  }

  // Put some data.
  for (size_t i = 0; i < children_name_id.size(); ++i) {
    for (size_t j = 0; j < children_name_id[i].size(); ++j) {
      ResourceEntry entry;
      entry.set_parent_resource_id(parents_id[i]);
      entry.set_base_name(children_name_id[i][j].first);
      entry.set_resource_id(children_name_id[i][j].second);
      EXPECT_TRUE(storage_->PutEntry(entry));
    }
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
  entry1.set_resource_id(parent_id1);
  ResourceEntry entry2;
  entry2.set_resource_id(child_id1);
  entry2.set_parent_resource_id(parent_id1);
  entry2.set_base_name(child_name1);

  // Put some data.
  EXPECT_TRUE(storage_->PutEntry(entry1));
  EXPECT_TRUE(storage_->PutEntry(entry2));

  // Close DB and reopen.
  storage_.reset(new ResourceMetadataStorage(temp_dir_.path()));
  ASSERT_TRUE(storage_->Initialize());

  // Can read data.
  scoped_ptr<ResourceEntry> result;
  result = storage_->GetEntry(parent_id1);
  ASSERT_TRUE(result);
  EXPECT_EQ(parent_id1, result->resource_id());

  result = storage_->GetEntry(child_id1);
  ASSERT_TRUE(result);
  EXPECT_EQ(child_id1, result->resource_id());
  EXPECT_EQ(parent_id1, result->parent_resource_id());
  EXPECT_EQ(child_name1, result->base_name());

  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));
}

TEST_F(ResourceMetadataStorageTest, IncompatibleDB) {
  const int64 kLargestChangestamp = 1234567890;
  const std::string key1 = "abcd";

  ResourceEntry entry;
  entry.set_resource_id(key1);

  // Put some data.
  EXPECT_TRUE(storage_->SetLargestChangestamp(kLargestChangestamp));
  EXPECT_TRUE(storage_->PutEntry(entry));
  EXPECT_TRUE(storage_->GetEntry(key1));

  // Set incompatible version and reopen DB.
  SetDBVersion(ResourceMetadataStorage::kDBVersion - 1);
  storage_.reset(new ResourceMetadataStorage(temp_dir_.path()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased because of the incompatible version.
  EXPECT_EQ(0, storage_->GetLargestChangestamp());
  EXPECT_FALSE(storage_->GetEntry(key1));
}

TEST_F(ResourceMetadataStorageTest, WrongPath) {
  // Create a file.
  base::FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &path));

  storage_.reset(new ResourceMetadataStorage(path));
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
  entry.set_resource_id(key1);
  entry.set_base_name(name1);
  EXPECT_TRUE(storage_->PutEntry(entry));
  EXPECT_TRUE(CheckValidity());

  // Put entry with key2 under key1.
  entry.set_resource_id(key2);
  entry.set_parent_resource_id(key1);
  entry.set_base_name(name2);
  EXPECT_TRUE(storage_->PutEntry(entry));
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
  entry.set_resource_id(key3);
  entry.set_parent_resource_id(key2);
  entry.set_base_name(name3);
  EXPECT_TRUE(storage_->PutEntry(entry));
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

}  // namespace drive
