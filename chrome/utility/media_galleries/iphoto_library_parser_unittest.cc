// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/common/media_galleries/iphoto_library.h"
#include "chrome/utility/media_galleries/iphoto_library_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SIMPLE_HEADER()                          \
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" \
    "<plist version=\"1.0\">"                    \
    "  <dict>"                                   \
    "    <key>Archive Path</key>"                \
    "    <string>/Users/username/px</string>"

#define ALBUMS_HEADER()             \
    "    <key>List of Albums</key>" \
    "    <array>"

#define ALBUMS_FOOTER() \
    "    </array>"

#define SIMPLE_ALBUM(id, name, photo1, photo2)         \
    "    <dict>"                                       \
    "      <key>AlbumId</key>"                         \
    "      <integer>" #id "</integer>"                 \
    "      <key>AlbumName</key>"                       \
    "      <string>" name "</string>"                  \
    "      <key>KeyList</key>"                         \
    "      <array>"                                    \
    "      <string>" #photo1 "</string>"               \
    "      <string>" #photo2 "</string>"               \
    "      </array>"                                   \
    "    </dict>"

#define IMAGE_LIST_HEADER()           \
    "   <key>Master Image List</key>" \
    "   <dict>"

#define IMAGE_LIST_FOOTER() \
    "   </dict>"

#define SIMPLE_PHOTO(id, guid, path, caption) \
    "  <key>" #id "</key>"                    \
    "  <dict>"                                \
    "    <key>MediaType</key>"                \
    "    <string>Image</string>"              \
    "    <key>Caption</key>"                  \
    "    <string>" caption "</string>"        \
    "    <key>GUID</key>"                     \
    "    <string>" #guid "</string>"          \
    "    <key>ModDateAsTimerInterval</key>"   \
    "    <string>386221543.0000</string>"     \
    "    <key>DateAsTimerInterval</key>"      \
    "    <string>386221543.0000</string>"     \
    "    <key>DateAsTimerIntervalGMT</key>"   \
    "    <string>385123456.00</string>"       \
    "    <key>ImagePath</key>"                \
    "    <string>" path "</string>"           \
    "    <key>OriginalPath</key>"             \
    "    <string>/original" path "</string>"  \
    "    <key>ThumbPath</key>"                \
    "    <string>" path "</string>"           \
    "  </dict>"

#define SIMPLE_FOOTER()  \
    "  </dict>"          \
    "</plist>"

 // Mismatched key/string tag at ImagePath.
#define MALFORMED_PHOTO1(id, guid, path, caption) \
    "  <key>" #id "</key>"             \
    "  <dict>"                         \
    "    <key>MediaType</key>"         \
    "    <string>Image</string>"       \
    "    <key>Caption<key>"            \
    "    <string>" caption "</string>" \
    "    <key>GUID</key>"              \
    "    <string>" #guid "</string>"   \
    "    <key>ImagePath</string>"      \
    "    <string>" path "</string>"    \
    "    <key>ThumbPath</key>"         \
    "    <string>" path "</string>"    \
    "  </dict>"

// Missing "<" delimiter at ImagePath.
#define MALFORMED_PHOTO2(id, guid, path, caption) \
    "  <key>" #id "</key>"             \
    "  <dict>"                         \
    "    <key>MediaType</key>"         \
    "    <string>Image</string>"       \
    "    <key>Caption<key>"            \
    "    <string>" caption "</string>" \
    "    <key>GUID</key>"              \
    "    <string>" #guid "</string>"   \
    "    <key>ImagePath/key>"          \
    "    <string>" path "</string>"    \
    "    <key>ThumbPath</key>"         \
    "    <string>" path "</string>"    \
    "  </dict>"

namespace iphoto {

namespace {

void ComparePhoto(const parser::Photo& a, const parser::Photo& b) {
  EXPECT_EQ(a.id, b.id);
  EXPECT_EQ(a.location.value(), b.location.value());
  EXPECT_EQ(a.original_location.value(), b.original_location.value());
}

void CompareAlbum(const parser::Album& a, const parser::Album& b) {
  EXPECT_EQ(a.size(), b.size());

  parser::Album::const_iterator a_it;
  parser::Album::const_iterator b_it;
  for (a_it = a.begin(), b_it = b.begin();
       a_it != a.end() && b_it != b.end();
       ++a_it, ++b_it) {
    EXPECT_EQ(*a_it, *b_it);
  }
}

void CompareAlbums(const parser::Albums& a, const parser::Albums& b) {
  EXPECT_EQ(a.size(), b.size());

  parser::Albums::const_iterator a_it;
  parser::Albums::const_iterator b_it;
  for (a_it = a.begin(), b_it = b.begin();
       a_it != a.end() && b_it != b.end();
       ++a_it, ++b_it) {
    EXPECT_EQ(a_it->first, b_it->first);
    CompareAlbum(a_it->second, b_it->second);
  }
}

void CompareLibrary(const parser::Library& a, const parser::Library& b) {
  CompareAlbums(a.albums, b.albums);

  std::set<parser::Photo>::const_iterator a_it;
  std::set<parser::Photo>::const_iterator b_it;
  for (a_it = a.all_photos.begin(), b_it = b.all_photos.begin();
       a_it != a.all_photos.end() && b_it != b.all_photos.end();
       ++a_it, ++b_it) {
    ComparePhoto(*a_it, *b_it);
  }
}

class IPhotoLibraryParserTest : public testing::Test {
 public:
  IPhotoLibraryParserTest() {}

  void TestParser(bool expected_result, const std::string& xml) {
    IPhotoLibraryParser parser;

    EXPECT_EQ(expected_result, parser.Parse(xml));
    if (!expected_result)
      return;

    CompareLibrary(expected_library_, parser.library());
  }

  void AddExpectedPhoto(uint32 id,
                        const std::string& location,
                        const std::string& album) {
    parser::Photo photo(id, base::FilePath::FromUTF8Unsafe(location),
                        base::FilePath::FromUTF8Unsafe("/original" + location));
    if (!album.empty())
      expected_library_.albums[album].insert(id);
    expected_library_.all_photos.insert(photo);
  }

 private:
  parser::Library expected_library_;

  DISALLOW_COPY_AND_ASSIGN(IPhotoLibraryParserTest);
};

TEST_F(IPhotoLibraryParserTest, EmptyLibrary) {
  TestParser(false, "");
}

TEST_F(IPhotoLibraryParserTest, MinimalXML) {
  AddExpectedPhoto(1, "/dir/Photo With Space.jpg", "");
  TestParser(
      true,
      SIMPLE_HEADER()
      IMAGE_LIST_HEADER()
      SIMPLE_PHOTO(1, 1, "/dir/Photo With Space.jpg", "Photo 1")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

TEST_F(IPhotoLibraryParserTest, MultiplePhotos) {
  AddExpectedPhoto(1, "/dir/SongA1.jpg", "");
  AddExpectedPhoto(2, "/dir/SongA2.jpg", "");
  AddExpectedPhoto(3, "/dir/SongA3.jpg", "");
  AddExpectedPhoto(4, "/dir/SongB1.jpg", "");
  AddExpectedPhoto(5, "/dir/SongB2.jpg", "");
  AddExpectedPhoto(6, "/dir2/SongB1.jpg", "");
  AddExpectedPhoto(7, "/dir2/SongB2.jpg", "");
  TestParser(
      true,
      SIMPLE_HEADER()
      IMAGE_LIST_HEADER()
      SIMPLE_PHOTO(1, 1, "/dir/SongA1.jpg", "Photo 1")
      SIMPLE_PHOTO(2, 2, "/dir/SongA2.jpg", "Photo 2")
      SIMPLE_PHOTO(3, 3, "/dir/SongA3.jpg", "Photo 3")
      SIMPLE_PHOTO(4, 4, "/dir/SongB1.jpg", "Photo 4")
      SIMPLE_PHOTO(5, 5, "/dir/SongB2.jpg", "Photo 5")
      SIMPLE_PHOTO(6, 6, "/dir2/SongB1.jpg", "Photo 6")
      SIMPLE_PHOTO(7, 7, "/dir2/SongB2.jpg", "Photo 7")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

TEST_F(IPhotoLibraryParserTest, Albums) {
  AddExpectedPhoto(1, "/dir/PhotoA1.jpg", "Album 1");
  AddExpectedPhoto(2, "/dir/PhotoA2.jpg", "Album 1");
  AddExpectedPhoto(3, "/dir/PhotoA3.jpg", "Album 2");
  AddExpectedPhoto(4, "/dir/PhotoB1.jpg", "Album 2");
  AddExpectedPhoto(5, "/dir/PhotoB2.jpg", "Album 3");
  AddExpectedPhoto(6, "/dir2/PhotoB1.jpg", "Album 3");
  AddExpectedPhoto(7, "/dir2/PhotoB2.jpg", "");
  TestParser(
      true,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      SIMPLE_ALBUM(10, "Album 1", 1, 2)
      SIMPLE_ALBUM(11, "Album 2", 3, 4)
      SIMPLE_ALBUM(11, "Album/3", 5, 6)
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      SIMPLE_PHOTO(1, 1, "/dir/PhotoA1.jpg", "Photo 1")
      SIMPLE_PHOTO(2, 2, "/dir/PhotoA2.jpg", "Photo 2")
      SIMPLE_PHOTO(3, 3, "/dir/PhotoA3.jpg", "Photo 3")
      SIMPLE_PHOTO(4, 4, "/dir/PhotoB1.jpg", "Photo 4")
      SIMPLE_PHOTO(5, 5, "/dir/PhotoB2.jpg", "Photo 5")
      SIMPLE_PHOTO(6, 6, "/dir2/PhotoB1.jpg", "Photo 6")
      SIMPLE_PHOTO(7, 7, "/dir2/PhotoB2.jpg", "Photo 7")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

TEST_F(IPhotoLibraryParserTest, MalformedStructure) {
  TestParser(
      false,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      ALBUMS_HEADER()
      ALBUMS_FOOTER()
      SIMPLE_FOOTER());

  TestParser(
      false,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      IMAGE_LIST_HEADER()
      SIMPLE_PHOTO(1, 1, "/bad.jpg", "p1")
      IMAGE_LIST_FOOTER()
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());

  TestParser(
      false,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      ALBUMS_HEADER()
      SIMPLE_PHOTO(1, 1, "/bad.jpg", "p1")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

TEST_F(IPhotoLibraryParserTest, MalformedSyntax) {
  TestParser(
      false,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      MALFORMED_PHOTO1(1, 1, "/bad.jpg", "p1")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());

  TestParser(
      false,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      MALFORMED_PHOTO2(1, 1, "/bad.jpg", "p1")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

TEST_F(IPhotoLibraryParserTest, DuplicateAlbumNames) {
  AddExpectedPhoto(1, "/dir/PhotoA1.jpg", "Album 1");
  AddExpectedPhoto(2, "/dir/PhotoA2.jpg", "Album 1");
  AddExpectedPhoto(3, "/dir/PhotoA3.jpg", "Album 1(11)");
  AddExpectedPhoto(4, "/dir/PhotoB1.jpg", "Album 1(11)");
  TestParser(
      true,
      SIMPLE_HEADER()
      ALBUMS_HEADER()
      SIMPLE_ALBUM(10, "Album 1", 1, 2)
      SIMPLE_ALBUM(11, "Album 1", 3, 4)
      ALBUMS_FOOTER()
      IMAGE_LIST_HEADER()
      SIMPLE_PHOTO(1, 1, "/dir/PhotoA1.jpg", "Photo 1")
      SIMPLE_PHOTO(2, 2, "/dir/PhotoA2.jpg", "Photo 2")
      SIMPLE_PHOTO(3, 3, "/dir/PhotoA3.jpg", "Photo 3")
      SIMPLE_PHOTO(4, 4, "/dir/PhotoB1.jpg", "Photo 4")
      IMAGE_LIST_FOOTER()
      SIMPLE_FOOTER());
}

}  // namespace

}  // namespace iphoto
