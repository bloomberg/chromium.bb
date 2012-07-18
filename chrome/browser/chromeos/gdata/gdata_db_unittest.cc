// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_db.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_db_factory.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace {

class GDataDBTest : public testing::Test {
 public:
  GDataDBTest() {
  }

  virtual ~GDataDBTest() {
  }

 protected:
  // testing::Test implementation.
  virtual void SetUp() OVERRIDE;

  // Tests GDataDB::GetPath and GDataDB::ResourceId, ensuring that an entry
  // matching |source| does not exist.
  void TestGetNotFound(const GDataEntry& source);

  // Tests GDataDB::GetPath and GDataDB::ResourceId, ensuring that an entry
  // matching |source| exists.
  void TestGetFound(const GDataEntry& source);

  // Tests GDataDB::GetPath and GDataDB::ResourceId, ensuring that an entry
  // matching |source| is corrupt.
  void TestGetCorrupt(const GDataEntry& source);

  // Initialize the database with the following entries:
  // drive/dir1
  // drive/dir2
  // drive/dir1/dir3
  // drive/dir1/file4
  // drive/dir1/file5
  // drive/dir2/file6
  // drive/dir2/file7
  // drive/dir2/file8
  // drive/dir1/dir3/file9
  // drive/dir1/dir3/file10
  void InitDB();

  // Helper functions to add a directory/file, incrementing index.
  GDataDirectory* AddDirectory(GDataDirectory* parent,
      GDataDirectoryService* root, int sequence_id);
  GDataFile* AddFile(GDataDirectory* parent,
      GDataDirectoryService* directory_service, int sequence_id);

  // Tests GDataDB::NewIterator and GDataDBIter::GetNext.
  // Creates an iterator with start at |parent|, and iterates comparing with
  // expected |filenames|.
  void TestIter(const std::string& parent,
                const char* file_paths[],
                size_t file_paths_size);

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<GDataDB> gdata_db_;
};

void GDataDBTest::SetUp() {
  profile_.reset(new TestingProfile());
  gdata_db_ = db_factory::CreateGDataDB(
      profile_->GetPath().Append("testdb"));
}

void GDataDBTest::TestGetNotFound(const GDataEntry& source) {
  scoped_ptr<GDataEntry> entry;
  GDataDB::Status status = gdata_db_->GetByPath(source.GetFilePath(), &entry);
  EXPECT_EQ(GDataDB::DB_KEY_NOT_FOUND, status);
  EXPECT_FALSE(entry.get());

  status = gdata_db_->GetByResourceId(source.resource_id(), &entry);
  EXPECT_EQ(GDataDB::DB_KEY_NOT_FOUND, status);
  EXPECT_FALSE(entry.get());
}

void GDataDBTest::TestGetFound(const GDataEntry& source) {
  scoped_ptr<GDataEntry> entry;
  GDataDB::Status status = gdata_db_->GetByPath(source.GetFilePath(), &entry);
  EXPECT_EQ(GDataDB::DB_OK, status);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(source.title(), entry->title());
  EXPECT_EQ(source.resource_id(), entry->resource_id());
  EXPECT_EQ(source.content_url(), entry->content_url());
  entry.reset();

  status = gdata_db_->GetByResourceId(source.resource_id(), &entry);
  EXPECT_EQ(GDataDB::DB_OK, status);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(source.title(), entry->title());
  EXPECT_EQ(source.resource_id(), entry->resource_id());
  EXPECT_EQ(source.content_url(), entry->content_url());
}

void GDataDBTest::TestGetCorrupt(const GDataEntry& source) {
  scoped_ptr<GDataEntry> entry;
  GDataDB::Status status = gdata_db_->GetByPath(source.GetFilePath(), &entry);
  EXPECT_EQ(GDataDB::DB_CORRUPTION, status);
  EXPECT_FALSE(entry.get());

  status = gdata_db_->GetByResourceId(source.resource_id(), &entry);
  EXPECT_EQ(GDataDB::DB_CORRUPTION, status);
  EXPECT_FALSE(entry.get());
}

void GDataDBTest::InitDB() {
  int sequence_id = 1;
  GDataDirectoryService directory_service;
  GDataDirectory* dir1 = AddDirectory(directory_service.root(),
      &directory_service, sequence_id++);
  GDataDirectory* dir2 = AddDirectory(directory_service.root(),
      &directory_service, sequence_id++);
  GDataDirectory* dir3 = AddDirectory(dir1, &directory_service, sequence_id++);

  AddFile(dir1, &directory_service, sequence_id++);
  AddFile(dir1, &directory_service, sequence_id++);

  AddFile(dir2, &directory_service, sequence_id++);
  AddFile(dir2, &directory_service, sequence_id++);
  AddFile(dir2, &directory_service, sequence_id++);

  AddFile(dir3, &directory_service, sequence_id++);
  AddFile(dir3, &directory_service, sequence_id++);
}

GDataDirectory* GDataDBTest::AddDirectory(
    GDataDirectory* parent,
    GDataDirectoryService* directory_service,
    int sequence_id) {
  GDataDirectory* dir = new GDataDirectory(parent, directory_service);
  const std::string dir_name = "dir" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("dir_resource_id:") +
                                  dir_name;
  dir->set_title(dir_name);
  dir->set_resource_id(resource_id);
  parent->AddEntry(dir);

  GDataDB::Status status = gdata_db_->Put(*dir);
  EXPECT_EQ(GDataDB::DB_OK, status);
  DVLOG(1) << "AddDirectory " << dir->GetFilePath().value()
           << ", " << resource_id;
  return dir;
}

GDataFile* GDataDBTest::AddFile(GDataDirectory* parent,
                                GDataDirectoryService* directory_service,
                                int sequence_id) {
  GDataFile* file = new GDataFile(parent, directory_service);
  const std::string title = "file" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("file_resource_id:") +
                                  title;
  file->set_title(title);
  file->set_resource_id(resource_id);
  parent->AddEntry(file);

  GDataDB::Status status = gdata_db_->Put(*file);
  EXPECT_EQ(GDataDB::DB_OK, status);
  DVLOG(1) << "AddFile " << file->GetFilePath().value()
           << ", " << resource_id;
  return file;
}

void GDataDBTest::TestIter(const std::string& parent,
                           const char* file_paths[],
                           size_t file_paths_size) {
  scoped_ptr<GDataDBIter> iter = gdata_db_->CreateIterator(
      FilePath::FromUTF8Unsafe(parent));
  for (size_t i = 0; ; ++i) {
    scoped_ptr<GDataEntry> entry;
    std::string path;
    if (!iter->GetNext(&path, &entry)) {
      EXPECT_EQ(i, file_paths_size);
      break;
    }
    ASSERT_LT(i, file_paths_size);
    // TODO(achuith): Also test entry->GetFilePath().
    EXPECT_EQ(FilePath(file_paths[i]).BaseName().value(), entry->file_name());
    EXPECT_EQ(file_paths[i], path);
    DVLOG(1) << "Iter " << path;
  }
}

}  // namespace

TEST_F(GDataDBTest, PutTest) {
  GDataDirectoryService directory_service;
  GDataDirectory dir(directory_service.root(), &directory_service);
  dir.set_title("dir");
  dir.set_file_name("dir");
  dir.set_resource_id("dir_resource_id");
  dir.set_content_url(GURL("http://content/dir"));
  dir.set_upload_url(GURL("http://upload/dir"));

  TestGetNotFound(dir);

  GDataDB::Status status = gdata_db_->Put(dir);
  ASSERT_EQ(GDataDB::DB_OK, status);

  TestGetFound(dir);

  scoped_ptr<GDataEntry> entry;
  status = gdata_db_->GetByPath(dir.GetFilePath(), &entry);
  ASSERT_EQ(GDataDB::DB_OK, status);
  EXPECT_EQ(dir.upload_url(), entry->AsGDataDirectory()->upload_url());
  EXPECT_TRUE(entry->AsGDataDirectory()->file_info().is_directory);

  status = gdata_db_->DeleteByPath(dir.GetFilePath());
  ASSERT_EQ(GDataDB::DB_OK, status);

  TestGetNotFound(dir);

  GDataFile file(&dir, &directory_service);
  file.set_title("file");
  file.set_file_name("file");
  file.set_resource_id("file_resource_id");
  file.set_content_url(GURL("http://content/dir/file"));
  file.set_file_md5("file_md5");

  TestGetNotFound(file);

  status = gdata_db_->Put(file);
  ASSERT_EQ(GDataDB::DB_OK, status);

  TestGetFound(file);

  status = gdata_db_->GetByPath(file.GetFilePath(), &entry);
  ASSERT_EQ(GDataDB::DB_OK, status);
  EXPECT_EQ(file.file_md5(), entry->AsGDataFile()->file_md5());
  EXPECT_FALSE(entry->AsGDataFile()->file_info().is_directory);

  status = gdata_db_->DeleteByPath(file.GetFilePath());
  ASSERT_EQ(GDataDB::DB_OK, status);

  TestGetNotFound(file);
}

TEST_F(GDataDBTest, IterTest) {
  InitDB();

  const char* dir1_children[] = {
    "drive/dir1",
    "drive/dir1/dir3",
    "drive/dir1/dir3/file10",
    "drive/dir1/dir3/file9",
    "drive/dir1/file4",
    "drive/dir1/file5",
  };
  TestIter("drive/dir1", dir1_children, arraysize(dir1_children));

  const char* dir2_children[] = {
    "drive/dir2",
    "drive/dir2/file6",
    "drive/dir2/file7",
    "drive/dir2/file8",
  };
  TestIter("drive/dir2", dir2_children, arraysize(dir2_children));

  const char* dir3_children[] = {
    "drive/dir1/dir3",
    "drive/dir1/dir3/file10",
    "drive/dir1/dir3/file9",
  };
  TestIter("drive/dir1/dir3", dir3_children, arraysize(dir3_children));

  const char* file10[] = {
    "drive/dir1/dir3/file10",
  };
  TestIter(file10[0], file10, arraysize(file10));

  const char* all_entries[] = {
    "drive/dir1",
    "drive/dir1/dir3",
    "drive/dir1/dir3/file10",
    "drive/dir1/dir3/file9",
    "drive/dir1/file4",
    "drive/dir1/file5",
    "drive/dir2",
    "drive/dir2/file6",
    "drive/dir2/file7",
    "drive/dir2/file8",
  };
  TestIter("", all_entries, arraysize(all_entries));

  TestIter("dir4", NULL, 0);
}

TEST_F(GDataDBTest, IncompatibleProtoTest) {
  GDataDirectoryService directory_service;
  GDataFile file(directory_service.root(), &directory_service);
  file.set_title("file");
  file.set_file_name("file");
  file.set_resource_id("file_resource_id");
  file.set_content_url(GURL("http://content/dir/file"));
  file.set_file_md5("file_md5");

  // Add a file and check if it's found.
  GDataDB::Status status = gdata_db_->Put(file);
  ASSERT_EQ(GDataDB::DB_OK, status);
  TestGetFound(file);
  // Check if the iterator works too.
  const char* all_entries[] = {
    "drive/file",
  };
  TestIter("", all_entries, arraysize(all_entries));

  // Tweak the file proto to simulate an incompatible proto in the DB.
  GDataFileProto file_proto;
  file.ToProto(&file_proto);
  // This will make FromProto() fail.
  file_proto.mutable_gdata_entry()->clear_upload_url();
  std::string serialized_proto;
  file_proto.SerializeToString(&serialized_proto);
  gdata_db_->PutRawForTesting("file_resource_id", serialized_proto);

  // Check if the corruption is detected.
  TestGetCorrupt(file);
  // We should no longer be able to find the file by iteration.
  TestIter("", NULL, 0);
}

}  // namespace gdata
