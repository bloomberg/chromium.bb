// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_db.h"

#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata_db_factory.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/test/base/testing_profile.h"

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

  // Initialize the database with the following entries:
  // dir1
  // dir2
  // dir1/dir3
  // dir1/file4
  // dir1/file5
  // dir2/file6
  // dir2/file7
  // dir2/file8
  // dir1/dir3/file9
  // dir1/dir3/file10
  void InitDB();

  // Helper functions to add a directory/file, incrementing index.
  GDataDirectory* AddDirectory(GDataDirectory* parent, int sequence_id);
  GDataFile* AddFile(GDataDirectory* parent, int sequence_id);

  // Tests GDataDB::NewIterator and GDataDBIter::GetNext.
  // Creates an iterator with start at |parent|, and iterates comparing with
  // expected |filenames|.
  void TestIter(const std::string& parent,
                const char* file_paths[],
                size_t file_paths_size);

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<GDataDB> gdata_db_;
  GDataRootDirectory root_;
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
  EXPECT_EQ(source.file_name(), entry->file_name());
  EXPECT_EQ(source.resource_id(), entry->resource_id());
  EXPECT_EQ(source.content_url(), entry->content_url());
  entry.reset();

  status = gdata_db_->GetByResourceId(source.resource_id(), &entry);
  EXPECT_EQ(GDataDB::DB_OK, status);
  ASSERT_TRUE(entry.get());
  EXPECT_EQ(source.file_name(), entry->file_name());
  EXPECT_EQ(source.resource_id(), entry->resource_id());
  EXPECT_EQ(source.content_url(), entry->content_url());
}

void GDataDBTest::InitDB() {
  int sequence_id = 1;
  GDataDirectory* dir1 = AddDirectory(NULL, sequence_id++);
  GDataDirectory* dir2 = AddDirectory(NULL, sequence_id++);
  GDataDirectory* dir3 = AddDirectory(dir1, sequence_id++);

  AddFile(dir1, sequence_id++);
  AddFile(dir1, sequence_id++);

  AddFile(dir2, sequence_id++);
  AddFile(dir2, sequence_id++);
  AddFile(dir2, sequence_id++);

  AddFile(dir3, sequence_id++);
  AddFile(dir3, sequence_id++);
}

GDataDirectory* GDataDBTest::AddDirectory(GDataDirectory* parent,
                                          int sequence_id) {
  GDataDirectory* dir = new GDataDirectory(parent ? parent : &root_, &root_);
  const std::string dir_name = "dir" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("dir_resource_id:") +
                                  dir_name;
  dir->set_file_name(dir_name);
  dir->set_resource_id(resource_id);
  GDataDB::Status status = gdata_db_->Put(*dir);
  EXPECT_EQ(GDataDB::DB_OK, status);
  DVLOG(1) << "AddDirectory " << dir->GetFilePath().value()
           << ", " << resource_id;
  return dir;
}

GDataFile* GDataDBTest::AddFile(GDataDirectory* parent,
                                int sequence_id) {
  GDataFile* file = new GDataFile(parent, &root_);
  const std::string file_name = "file" + base::IntToString(sequence_id);
  const std::string resource_id = std::string("file_resource_id:") +
                                  file_name;
  file->set_file_name(file_name);
  file->set_resource_id(resource_id);
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
  GDataDirectory* dir = new GDataDirectory(&root_, &root_);
  dir->set_file_name("dir");
  dir->set_resource_id("dir_resource_id");
  dir->set_content_url(GURL("http://content/dir"));
  dir->set_upload_url(GURL("http://upload/dir"));

  TestGetNotFound(*dir);

  GDataDB::Status status = gdata_db_->Put(*dir);
  EXPECT_EQ(GDataDB::DB_OK, status);

  TestGetFound(*dir);

  scoped_ptr<GDataEntry> entry;
  gdata_db_->GetByPath(dir->GetFilePath(), &entry);
  EXPECT_EQ(dir->upload_url(), entry->AsGDataDirectory()->upload_url());
  EXPECT_TRUE(entry->AsGDataDirectory()->file_info().is_directory);

  status = gdata_db_->DeleteByPath(dir->GetFilePath());
  EXPECT_EQ(GDataDB::DB_OK, status);

  TestGetNotFound(*dir);

  GDataFile* file = new GDataFile(dir, &root_);
  file->set_file_name("file1");
  file->set_resource_id("file1_resource_id");
  file->set_content_url(GURL("http://content/dir1/file1"));
  file->set_file_md5("file1_md5");

  TestGetNotFound(*file);

  status = gdata_db_->Put(*file);
  EXPECT_EQ(GDataDB::DB_OK, status);

  TestGetFound(*file);

  gdata_db_->GetByPath(file->GetFilePath(), &entry);
  EXPECT_EQ(file->file_md5(), entry->AsGDataFile()->file_md5());
  EXPECT_FALSE(entry->AsGDataFile()->file_info().is_directory);

  status = gdata_db_->DeleteByPath(file->GetFilePath());
  EXPECT_EQ(GDataDB::DB_OK, status);

  TestGetNotFound(*file);
}

TEST_F(GDataDBTest, IterTest) {
  InitDB();

  const char* dir1_children[] = {
    "dir1",
    "dir1/dir3",
    "dir1/dir3/file10",
    "dir1/dir3/file9",
    "dir1/file4",
    "dir1/file5",
  };
  TestIter("dir1", dir1_children, arraysize(dir1_children));

  const char* dir2_children[] = {
    "dir2",
    "dir2/file6",
    "dir2/file7",
    "dir2/file8",
  };
  TestIter("dir2", dir2_children, arraysize(dir2_children));

  const char* dir3_children[] = {
    "dir1/dir3",
    "dir1/dir3/file10",
    "dir1/dir3/file9",
  };
  TestIter("dir1/dir3", dir3_children, arraysize(dir3_children));

  const char* file10[] = {
    "dir1/dir3/file10",
  };
  TestIter(file10[0], file10, arraysize(file10));

  const char* all_entries[] = {
    "dir1",
    "dir1/dir3",
    "dir1/dir3/file10",
    "dir1/dir3/file9",
    "dir1/file4",
    "dir1/file5",
    "dir2",
    "dir2/file6",
    "dir2/file7",
    "dir2/file8",
  };
  TestIter("", all_entries, arraysize(all_entries));

  TestIter("dir4", NULL, 0);
}

}  // namespace gdata
