// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveResourceMetadataStorageTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    storage_.reset(new DriveResourceMetadataStorageDB(temp_dir_.path()));
    ASSERT_TRUE(storage_->Initialize());
  }

  virtual void TearDown() OVERRIDE {
  }

  // Overwrites |storage_|'s version.
  void SetDBVersion(int version) {
    scoped_ptr<DriveResourceMetadataHeader> header = storage_->GetHeader();
    ASSERT_TRUE(header);
    header->set_version(version);
    storage_->PutHeader(*header);
  }

  base::ScopedTempDir temp_dir_;
  scoped_ptr<DriveResourceMetadataStorageDB> storage_;
};

TEST_F(DriveResourceMetadataStorageTest, LargestChangestamp) {
  const int64 kLargestChangestamp = 1234567890;
  storage_->SetLargestChangestamp(kLargestChangestamp);
  EXPECT_EQ(kLargestChangestamp, storage_->GetLargestChangestamp());
}

TEST_F(DriveResourceMetadataStorageTest, PutEntry) {
  const std::string key1 = "abcdefg";
  const std::string key2 = "abcd";

  DriveEntryProto entry1;
  entry1.set_resource_id(key1);

  // key1 not found.
  EXPECT_FALSE(storage_->GetEntry(key1));

  // Put entry1.
  storage_->PutEntry(entry1);

  // key1 found.
  scoped_ptr<DriveEntryProto> result;
  result = storage_->GetEntry(key1);
  ASSERT_TRUE(result);
  EXPECT_EQ(key1, result->resource_id());

  // key2 not found.
  EXPECT_FALSE(storage_->GetEntry(key2));

  // Remove key1.
  storage_->RemoveEntry(key1);
  EXPECT_FALSE(storage_->GetEntry(key1));
}

TEST_F(DriveResourceMetadataStorageTest, PutChild) {
  const std::string parent_id1 = "abcdefg";
  const std::string parent_id2 = "abcd";
  const std::string child_name1 = "WXYZABC";
  const std::string child_name2 = "efgWXYZABC";
  const std::string child_id1 = "qwerty";
  const std::string child_id2 = "asdfgh";

  // No child found.
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name2).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name2).empty());

  // Put child1 under parent1.
  storage_->PutChild(parent_id1, child_name1, child_id1);
  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name2).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name2).empty());

  // Put child2 under parent1.
  storage_->PutChild(parent_id1, child_name2, child_id2);
  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));
  EXPECT_EQ(child_id2, storage_->GetChild(parent_id1, child_name2));
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name2).empty());

  // Get children.
  std::vector<std::string> children;
  storage_->GetChildren(parent_id1, &children);
  EXPECT_EQ(2U, children.size());
  EXPECT_TRUE(std::find(children.begin(), children.end(), child_id1)
              != children.end());
  EXPECT_TRUE(std::find(children.begin(), children.end(), child_id2)
              != children.end());

  // Remove child1.
  storage_->RemoveChild(parent_id1, child_name1);
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name1).empty());
  EXPECT_EQ(child_id2, storage_->GetChild(parent_id1, child_name2));
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name2).empty());

  // Remove child2.
  storage_->RemoveChild(parent_id1, child_name2);
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id1, child_name2).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name1).empty());
  EXPECT_TRUE(storage_->GetChild(parent_id2, child_name2).empty());
}

TEST_F(DriveResourceMetadataStorageTest, OpenExistingDB) {
  const std::string parent_id1 = "abcdefg";
  const std::string child_name1 = "WXYZABC";
  const std::string child_id1 = "qwerty";

  DriveEntryProto entry1;
  entry1.set_resource_id(parent_id1);

  // Put some data.
  storage_->PutEntry(entry1);
  storage_->PutChild(parent_id1, child_name1, child_id1);
  scoped_ptr<DriveEntryProto> result = storage_->GetEntry(parent_id1);
  ASSERT_TRUE(result);
  EXPECT_EQ(parent_id1, result->resource_id());
  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));

  // Close DB and reopen.
  storage_.reset(new DriveResourceMetadataStorageDB(temp_dir_.path()));
  ASSERT_TRUE(storage_->Initialize());

  // Can read data.
  result = storage_->GetEntry(parent_id1);
  ASSERT_TRUE(result);
  EXPECT_EQ(parent_id1, result->resource_id());
  EXPECT_EQ(child_id1, storage_->GetChild(parent_id1, child_name1));
}

TEST_F(DriveResourceMetadataStorageTest, IncompatibleDB) {
  const int64 kLargestChangestamp = 1234567890;
  const std::string key1 = "abcd";

  DriveEntryProto entry;
  entry.set_resource_id(key1);

  // Put some data.
  storage_->SetLargestChangestamp(kLargestChangestamp);
  storage_->PutEntry(entry);
  EXPECT_TRUE(storage_->GetEntry(key1));

  // Set incompatible version and reopen DB.
  SetDBVersion(DriveResourceMetadataStorageDB::kDBVersion - 1);
  storage_.reset(new DriveResourceMetadataStorageDB(temp_dir_.path()));
  ASSERT_TRUE(storage_->Initialize());

  // Data is erased because of the incompatible version.
  EXPECT_EQ(0, storage_->GetLargestChangestamp());
  EXPECT_FALSE(storage_->GetEntry(key1));
}

TEST_F(DriveResourceMetadataStorageTest, WrongPath) {
  // Create a file.
  base::FilePath path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), &path));

  storage_.reset(new DriveResourceMetadataStorageDB(path));
  // Cannot initialize DB beacause the path does not point a directory.
  ASSERT_FALSE(storage_->Initialize());
}

}  // namespace drive
