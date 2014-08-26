// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media_galleries/picasa_test_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/common/media_galleries/pmp_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace picasa {

void WriteAlbumTable(const base::FilePath& column_file_destination,
                     const std::vector<uint32>& category_vector,
                     const std::vector<double>& date_vector,
                     const std::vector<std::string>& filename_vector,
                     const std::vector<std::string>& name_vector,
                     const std::vector<std::string>& token_vector,
                     const std::vector<std::string>& uid_vector) {
  ASSERT_TRUE(PmpTestUtil::WriteIndicatorFile(
      column_file_destination, kPicasaAlbumTableName));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "category",
      PMP_TYPE_UINT32, category_vector));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "date",
      PMP_TYPE_DOUBLE64, date_vector));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "filename",
      PMP_TYPE_STRING, filename_vector));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "name",
      PMP_TYPE_STRING, name_vector));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "token",
      PMP_TYPE_STRING, token_vector));
  ASSERT_TRUE(PmpTestUtil::WriteColumnFileFromVector(
      column_file_destination, kPicasaAlbumTableName, "uid",
      PMP_TYPE_STRING, uid_vector));
}

void WriteTestAlbumTable(
    const base::FilePath& column_file_destination,
    const base::FilePath& test_folder_1_path,
    const base::FilePath& test_folder_2_path) {
  std::vector<uint32> category_vector;
  category_vector.push_back(kAlbumCategoryFolder);
  category_vector.push_back(kAlbumCategoryInvalid);
  category_vector.push_back(kAlbumCategoryAlbum);
  category_vector.push_back(kAlbumCategoryFolder);
  category_vector.push_back(kAlbumCategoryAlbum);

  std::vector<double> date_vector;
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);
  date_vector.push_back(0.0);

  std::vector<std::string> filename_vector;
  filename_vector.push_back(test_folder_1_path.AsUTF8Unsafe());
  filename_vector.push_back("");
  filename_vector.push_back("");
  filename_vector.push_back(test_folder_2_path.AsUTF8Unsafe());
  filename_vector.push_back("");

  std::vector<std::string> name_vector;
  name_vector.push_back(test_folder_1_path.BaseName().AsUTF8Unsafe());
  name_vector.push_back("");
  name_vector.push_back("Album 1 Name");
  name_vector.push_back(test_folder_2_path.BaseName().AsUTF8Unsafe());
  name_vector.push_back("Album 2 Name");

  std::vector<std::string> token_vector;
  token_vector.push_back("");
  token_vector.push_back("");
  token_vector.push_back(std::string(kAlbumTokenPrefix) + "uid3");
  token_vector.push_back("");
  token_vector.push_back(std::string(kAlbumTokenPrefix) + "uid5");

  std::vector<std::string> uid_vector;
  uid_vector.push_back("uid1");
  uid_vector.push_back("uid2");
  uid_vector.push_back("uid3");
  uid_vector.push_back("uid4");
  uid_vector.push_back("uid5");

  WriteAlbumTable(column_file_destination, category_vector, date_vector,
                  filename_vector, name_vector, token_vector, uid_vector);
}

void WriteTestAlbumsImagesIndex(const base::FilePath& test_folder_1_path,
                                const base::FilePath& test_folder_2_path) {
  const char folder_1_test_ini[] =
      "[InBoth.jpg]\n"
      "albums=uid3,uid5\n"
      "[InSecondAlbumOnly.jpg]\n"
      "albums=uid5\n";
  ASSERT_TRUE(
      base::WriteFile(test_folder_1_path.AppendASCII(kPicasaINIFilename),
                      folder_1_test_ini,
                      arraysize(folder_1_test_ini)));

  const char folder_2_test_ini[] =
      "[InFirstAlbumOnly.jpg]\n"
      "albums=uid3\n";
  ASSERT_TRUE(
      base::WriteFile(test_folder_2_path.AppendASCII(kPicasaINIFilename),
                      folder_2_test_ini,
                      arraysize(folder_2_test_ini)));
}

}  // namespace picasa
