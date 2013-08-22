// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/utility/media_galleries/picasa_albums_indexer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace picasa {

namespace {

const char test_ini_string[] =
    "[.album:e66fb059001aabcc69b262b7009fad90]\n"
    "name=CustomAlbum1\n"
    "token=e66fb059001aabcc69b262b7009fad90\n"
    "date=2013-03-15T14:53:21-07:00\n"
    "[InBoth.jpg]\n"
    "albums=e66fb059001aabcc69b262b7009fad90,18cb2df48aaa98e1c276b45cfcd81c95\n"
    "[.album:18cb2df48aaa98e1c276b45cfcd81c95]\n"
    "name=CustomAlbum2\n"
    "token=18cb2df48aaa98e1c276b45cfcd81c95\n"
    "date=2013-04-01T16:37:34-07:00\n"
    "[InFirst.jpg]\n"
    "albums=e66fb059001aabcc69b262b7009fad90\n"
    "[InSecond.jpg]\n"
    "albums=18cb2df48aaa98e1c276b45cfcd81c95\n";

TEST(PicasaAlbumsIndexerTest, ValidCase) {
  std::set<std::string> valid_album_uuids;
  valid_album_uuids.insert("e66fb059001aabcc69b262b7009fad90");
  valid_album_uuids.insert("18cb2df48aaa98e1c276b45cfcd81c95");
  PicasaAlbumsIndexer indexer(valid_album_uuids);

  std::vector<FolderINIContents> folders_inis;
  FolderINIContents folder_ini;
  folder_ini.folder_path = base::FilePath(FILE_PATH_LITERAL("/foo/baz"));
  folder_ini.ini_contents = test_ini_string;
  folders_inis.push_back(folder_ini);
  indexer.ParseFolderINI(folders_inis);

  const AlbumImagesMap& albums_images = indexer.albums_images();
  AlbumImagesMap::const_iterator it;

  it = albums_images.find("e66fb059001aabcc69b262b7009fad90");
  ASSERT_NE(albums_images.end(), it);
  EXPECT_EQ(2u, it->second.size());
  EXPECT_EQ(1u, it->second.count("InBoth.jpg"));
  EXPECT_EQ(1u, it->second.count("InFirst.jpg"));

  it = albums_images.find("18cb2df48aaa98e1c276b45cfcd81c95");
  ASSERT_NE(albums_images.end(), it);
  EXPECT_EQ(2u, it->second.size());
  EXPECT_EQ(1u, it->second.count("InBoth.jpg"));
  EXPECT_EQ(1u, it->second.count("InSecond.jpg"));
}

TEST(PicasaAlbumsIndexerTest, DedupeNames) {
  std::set<std::string> valid_album_uuids;
  valid_album_uuids.insert("e66fb059001aabcc69b262b7009fad90");
  valid_album_uuids.insert("18cb2df48aaa98e1c276b45cfcd81c95");
  PicasaAlbumsIndexer indexer(valid_album_uuids);

  std::vector<FolderINIContents> folders_inis;
  FolderINIContents folder_ini;
  folder_ini.folder_path = base::FilePath(FILE_PATH_LITERAL("/foo/baz"));
  folder_ini.ini_contents = test_ini_string;
  folders_inis.push_back(folder_ini);

  // Add a second folder with just a duplicate InFirst.jpg.
  folder_ini.folder_path = base::FilePath(FILE_PATH_LITERAL("/foo/second"));
  folder_ini.ini_contents =
      "[.album:e66fb059001aabcc69b262b7009fad90]\n"
      "name=CustomAlbum1\n"
      "token=e66fb059001aabcc69b262b7009fad90\n"
      "date=2013-03-15T14:53:21-07:00\n"
      "[InFirst.jpg]\n"
      "albums=e66fb059001aabcc69b262b7009fad90\n";
  folders_inis.push_back(folder_ini);

  // Add a third folder with just a duplicate InFirst.jpg and InSecond.jpg.
  folder_ini.folder_path = base::FilePath(FILE_PATH_LITERAL("/foo/third"));
  folder_ini.ini_contents =
      "[.album:e66fb059001aabcc69b262b7009fad90]\n"
      "name=CustomAlbum1\n"
      "token=e66fb059001aabcc69b262b7009fad90\n"
      "date=2013-03-15T14:53:21-07:00\n"
      "[.album:18cb2df48aaa98e1c276b45cfcd81c95]\n"
      "name=CustomAlbum2\n"
      "token=18cb2df48aaa98e1c276b45cfcd81c95\n"
      "date=2013-04-01T16:37:34-07:00\n"
      "[InFirst.jpg]\n"
      "albums=e66fb059001aabcc69b262b7009fad90\n"
      "[InSecond.jpg]\n"
      "albums=18cb2df48aaa98e1c276b45cfcd81c95\n";
  folders_inis.push_back(folder_ini);

  indexer.ParseFolderINI(folders_inis);

  const AlbumImagesMap& albums_images = indexer.albums_images();
  AlbumImagesMap::const_iterator it;

  it = albums_images.find("e66fb059001aabcc69b262b7009fad90");
  ASSERT_NE(albums_images.end(), it);
  EXPECT_EQ(4u, it->second.size());
  EXPECT_EQ(1u, it->second.count("InBoth.jpg"));
  EXPECT_EQ(1u, it->second.count("InFirst.jpg"));
  EXPECT_EQ(1u, it->second.count("InFirst (1).jpg"));
  EXPECT_EQ(1u, it->second.count("InFirst (2).jpg"));

  it = albums_images.find("18cb2df48aaa98e1c276b45cfcd81c95");
  ASSERT_NE(albums_images.end(), it);
  EXPECT_EQ(3u, it->second.size());
  EXPECT_EQ(1u, it->second.count("InBoth.jpg"));
  EXPECT_EQ(1u, it->second.count("InSecond.jpg"));
  EXPECT_EQ(1u, it->second.count("InSecond (1).jpg"));
}

TEST(PicasaAlbumsIndexerTest, EdgeCases) {
  std::set<std::string> valid_album_uuids;
  valid_album_uuids.insert("album-uuid-with-no-images");
  valid_album_uuids.insert("18cb2df48aaa98e1c276b45cfcd81c95");
  PicasaAlbumsIndexer indexer(valid_album_uuids);

  std::vector<FolderINIContents> folders_inis;
  FolderINIContents folder_ini;
  folder_ini.folder_path = base::FilePath(FILE_PATH_LITERAL("/foo/baz"));
  folder_ini.ini_contents = test_ini_string;
  folders_inis.push_back(folder_ini);
  indexer.ParseFolderINI(folders_inis);

  const AlbumImagesMap& albums_images = indexer.albums_images();
  AlbumImagesMap::const_iterator it;

  // UUID that exists in INI that isn't in |valid_album_uuids|.
  EXPECT_EQ(albums_images.end(),
            albums_images.find("e66fb059001aabcc69b262b7009fad90"));

  // UUID that exists in INI and |valid_album_uuids| but no images.
  it = albums_images.find("album-uuid-with-no-images");
  EXPECT_NE(albums_images.end(), it);
  EXPECT_EQ(0u, it->second.size());

  // Should still parse normal albums correctly.
  it = albums_images.find("18cb2df48aaa98e1c276b45cfcd81c95");
  EXPECT_NE(albums_images.end(), it);
  EXPECT_EQ(2u, it->second.size());
  EXPECT_EQ(1u, it->second.count("InBoth.jpg"));
  EXPECT_EQ(1u, it->second.count("InSecond.jpg"));
}

}  // namespace

}  // namespace picasa
