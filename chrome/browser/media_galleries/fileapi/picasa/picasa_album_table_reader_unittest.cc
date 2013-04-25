// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa/picasa_album_table_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using picasaimport::PmpTestHelper;
using picasaimport::PMP_TYPE_UINT32;

TEST(PicasaAlbumTableReaderTest, FoldersAndAlbums) {
  PmpTestHelper test_helper(picasaimport::kPicasaAlbumTableName);
  ASSERT_TRUE(test_helper.Init());

  int test_time_delta = 100;

  std::vector<uint32> category_vector;
  category_vector.push_back(picasaimport::kAlbumCategoryFolder);
  category_vector.push_back(picasaimport::kAlbumCategoryInvalid);
  category_vector.push_back(picasaimport::kAlbumCategoryUserAlbum);

  std::vector<double> date_vector;
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);
  date_vector.push_back(0.0 + test_time_delta);

  std::string test_folder_name = "Pix4dalulz";
  std::string test_user_album_name = "Cats";

  base::FilePath test_folder_path =
      base::FilePath(base::FilePath::FromUTF8Unsafe("C:\\Pix4dalulz"));

  // Only folders require filenames. Tests handling of different length columns.
  std::vector<std::string> filename_vector;
  filename_vector.push_back(test_folder_path.AsUTF8Unsafe());

  std::vector<std::string> name_vector;
  name_vector.push_back(test_folder_name);
  name_vector.push_back("");
  name_vector.push_back(test_user_album_name);

  std::vector<std::string> token_vector;
  token_vector.push_back("");
  token_vector.push_back("");
  token_vector.push_back(std::string(picasaimport::kAlbumTokenPrefix) + "uid3");

  std::vector<std::string> uid_vector;
  uid_vector.push_back("uid1");
  uid_vector.push_back("uid2");
  uid_vector.push_back("uid3");

  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "category", picasaimport::PMP_TYPE_UINT32, category_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "date", picasaimport::PMP_TYPE_DOUBLE64, date_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "filename", picasaimport::PMP_TYPE_STRING, filename_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "name", picasaimport::PMP_TYPE_STRING, name_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "token", picasaimport::PMP_TYPE_STRING, token_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      "uid", picasaimport::PMP_TYPE_STRING, uid_vector));

  picasaimport::PicasaAlbumTableReader reader(test_helper.GetTempDirPath());

  ASSERT_TRUE(reader.Init());

  const std::vector<picasaimport::FolderInfo>& folders = reader.folders();
  const std::vector<picasaimport::AlbumInfo>& user_albums =
      reader.user_albums();

  ASSERT_EQ(1u, folders.size());
  ASSERT_EQ(1u, user_albums.size());

  EXPECT_EQ(test_folder_name, folders[0].name);
  EXPECT_EQ(test_user_album_name, user_albums[0].name);

  EXPECT_EQ(test_folder_path, folders[0].path);

  base::TimeDelta time_delta = user_albums[0].timestamp - folders[0].timestamp;

  EXPECT_EQ(test_time_delta, time_delta.InDays());
}

}  // namespace
