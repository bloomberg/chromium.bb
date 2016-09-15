// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/picasa_album_table_reader.h"

#include <stdint.h>

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "chrome/common/media_galleries/picasa_test_util.h"
#include "chrome/common/media_galleries/pmp_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace picasa {

namespace {

TEST(PicasaAlbumTableReaderTest, FoldersAndAlbums) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int test_time_delta = 100;

  std::vector<uint32_t> category_vector;
  category_vector.push_back(kAlbumCategoryFolder);
  category_vector.push_back(kAlbumCategoryInvalid);
  category_vector.push_back(kAlbumCategoryAlbum);

  std::vector<double> date_vector;
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);
  date_vector.push_back(0.0 + test_time_delta);

  std::string test_folder_name = "Pix4dalulz";
  std::string test_album_name = "Cats";

  base::FilePath test_folder_path =
      base::FilePath(base::FilePath::FromUTF8Unsafe("C:\\Pix4dalulz"));

  // Only folders require filenames. Tests handling of different length columns.
  std::vector<std::string> filename_vector;
  filename_vector.push_back(test_folder_path.AsUTF8Unsafe());

  std::vector<std::string> name_vector;
  name_vector.push_back(test_folder_name);
  name_vector.push_back("");
  name_vector.push_back(test_album_name);

  std::vector<std::string> token_vector;
  token_vector.push_back("");
  token_vector.push_back("");
  token_vector.push_back(std::string(kAlbumTokenPrefix) + "uid3");

  std::vector<std::string> uid_vector;
  uid_vector.push_back("uid1");
  uid_vector.push_back("uid2");
  uid_vector.push_back("uid3");

  WriteAlbumTable(temp_dir.GetPath(), category_vector, date_vector,
                  filename_vector, name_vector, token_vector, uid_vector);

  AlbumTableFiles album_table_files(temp_dir.GetPath());
  PicasaAlbumTableReader reader(std::move(album_table_files));

  ASSERT_TRUE(reader.Init());

  const std::vector<AlbumInfo>& albums = reader.albums();
  const std::vector<AlbumInfo>& folders = reader.folders();

  ASSERT_EQ(1u, albums.size());
  ASSERT_EQ(1u, folders.size());

  EXPECT_EQ(test_album_name, albums[0].name);
  EXPECT_EQ(test_folder_name, folders[0].name);

  EXPECT_EQ(test_folder_path, folders[0].path);

  base::TimeDelta time_delta = albums[0].timestamp - folders[0].timestamp;

  EXPECT_EQ(test_time_delta, time_delta.InDays());
}

}  // namespace

}  // namespace picasa
