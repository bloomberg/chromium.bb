// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/media_gallery/media_gallery_database.h"
#include "chrome/browser/media_gallery/media_gallery_database_types.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {

class MediaGalleryDatabaseTest : public testing::Test,
                                 public MediaGalleryDatabase {
 public:
  MediaGalleryDatabaseTest() { }

 protected:
  virtual sql::Connection& GetDB() {
    return db_;
  }

 private:
  // Test setup.
  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath db_file = temp_dir_.path().AppendASCII("MediaGalleryTest.db");

    ASSERT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    ASSERT_EQ(sql::INIT_OK, InitInternal(&db_));
  }

  void TearDown() {
    db_.Close();
  }

  ScopedTempDir temp_dir_;
  sql::Connection db_;
};

TEST_F(MediaGalleryDatabaseTest, Init) {
  EXPECT_TRUE(DoesCollectionsTableExist(&GetDB()));
}

TEST_F(MediaGalleryDatabaseTest, AddCollection) {
  CollectionRow row1(FilePath(FILE_PATH_LITERAL("path1")),
                     base::Time::FromDoubleT(12345),
                     123,
                     false);
  CollectionId rowid = CreateCollectionRow(&row1);
  EXPECT_EQ(rowid, row1.id);

  CollectionRow row2;
  EXPECT_TRUE(GetCollectionRow(rowid, &row2));
  EXPECT_TRUE(row1 == row2);
}

TEST_F(MediaGalleryDatabaseTest, StandardizePath) {
  FilePath path(FILE_PATH_LITERAL("path1"));
  path = path.AppendASCII("path2");
  CollectionRow row1(path,
                     base::Time::FromDoubleT(12345),
                     123,
                     false);
  CollectionId rowid = CreateCollectionRow(&row1);
  EXPECT_EQ(rowid, row1.id);

  CollectionRow row2;
  EXPECT_TRUE(GetCollectionRow(rowid, &row2));
  EXPECT_EQ(FILE_PATH_LITERAL("path1/path2"), row2.path.value());
}

}  // namespace chrome
