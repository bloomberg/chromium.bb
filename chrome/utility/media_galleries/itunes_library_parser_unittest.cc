// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "chrome/utility/media_galleries/itunes_library_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SIMPLE_HEADER()         \
    "<plist>"                   \
    "  <dict>"                  \
    "    <key>Tracks</key>"     \
    "    <dict>"

#define SIMPLE_TRACK(key, id, path, artist, album)                     \
    "<key>" #key "</key>"                                              \
    "<dict>"                                                           \
    "  <key>Track ID</key><integer>" #id "</integer>"                  \
    "  <key>Location</key><string>file://localhost/" path "</string>"  \
    "  <key>Artist</key><string>" artist "</string>"                   \
    "  <key>Album</key><string>" album "</string>"                     \
    "</dict>"

#define SIMPLE_FOOTER()  \
    "    </dict>"        \
    "  </dict>"          \
    "</plist>"

namespace itunes {

namespace {

void CompareTrack(const parser::Track& a, const parser::Track& b) {
  EXPECT_EQ(a.id, b.id);
  EXPECT_EQ(a.location.value(), b.location.value());
}

void CompareAlbum(const parser::Album& a, const parser::Album& b) {
  EXPECT_EQ(a.size(), b.size());

  parser::Album::const_iterator a_it;
  parser::Album::const_iterator b_it;
  for (a_it = a.begin(), b_it = b.begin();
       a_it != a.end() && b_it != b.end();
       ++a_it, ++b_it) {
    CompareTrack(*a_it, *b_it);
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
  EXPECT_EQ(a.size(), b.size());

  parser::Library::const_iterator a_it;
  parser::Library::const_iterator b_it;
  for (a_it = a.begin(), b_it = b.begin();
       a_it != a.end() && b_it != b.end();
       ++a_it, ++b_it) {
    EXPECT_EQ(a_it->first, b_it->first);
    CompareAlbums(a_it->second, b_it->second);
  }
}

class ITunesLibraryParserTest : public testing::Test {
 public:
  ITunesLibraryParserTest() {}

  void TestParser(bool expected_result, const std::string& xml) {
    ITunesLibraryParser parser;

    EXPECT_EQ(expected_result, parser.Parse(xml));
    if (!expected_result)
      return;

    CompareLibrary(expected_library_, parser.library());
  }

  void AddExpectedTrack(uint32 id, const std::string& location,
                        const std::string& artist, const std::string& album) {
    // On Mac this pretends that C: is a directory.
#if defined(OS_MACOSX)
    std::string os_location = "/" + location;
#else
    const std::string& os_location = location;
#endif
    parser::Track track(id, base::FilePath::FromUTF8Unsafe(os_location));
    expected_library_[artist][album].insert(track);
  }

 private:
  parser::Library expected_library_;

  DISALLOW_COPY_AND_ASSIGN(ITunesLibraryParserTest);
};

TEST_F(ITunesLibraryParserTest, EmptyLibrary) {
  TestParser(false, "");
}

TEST_F(ITunesLibraryParserTest, MinimalXML) {
  AddExpectedTrack(1, "C:/dir/Song With Space.mp3", "Artist A", "Album A");
  TestParser(
      true,
      SIMPLE_HEADER()
      SIMPLE_TRACK(1, 1, "C:/dir/Song%20With%20Space.mp3", "Artist A",
                   "Album A")
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, MultipleSongs) {
  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Album A");
  AddExpectedTrack(2, "C:/dir/SongA2.mp3", "Artist A", "Album A");
  AddExpectedTrack(3, "C:/dir/SongA3.mp3", "Artist A", "Album A");
  AddExpectedTrack(4, "C:/dir/SongB1.mp3", "Artist A", "Album B");
  AddExpectedTrack(5, "C:/dir/SongB2.mp3", "Artist A", "Album B");
  AddExpectedTrack(6, "C:/dir2/SongB1.mp3", "Artist B", "Album B");
  AddExpectedTrack(7, "C:/dir2/SongB2.mp3", "Artist B", "Album B");
  TestParser(
      true,
      SIMPLE_HEADER()
      SIMPLE_TRACK(1, 1, "C:/dir/SongA1.mp3", "Artist A", "Album A")
      SIMPLE_TRACK(2, 2, "C:/dir/SongA2.mp3", "Artist A", "Album A")
      SIMPLE_TRACK(3, 3, "C:/dir/SongA3.mp3", "Artist A", "Album A")
      SIMPLE_TRACK(4, 4, "C:/dir/SongB1.mp3", "Artist A", "Album B")
      SIMPLE_TRACK(5, 5, "C:/dir/SongB2.mp3", "Artist A", "Album B")
      SIMPLE_TRACK(6, 6, "C:/dir2/SongB1.mp3", "Artist B", "Album B")
      SIMPLE_TRACK(7, 7, "C:/dir2/SongB2.mp3", "Artist B", "Album B")
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, MismatchedId) {
  TestParser(
      false,
      SIMPLE_HEADER()
      SIMPLE_TRACK(1, 2, "C:/dir/SongA1.mp3", "Artist A", "Album A")
      SIMPLE_FOOTER());

  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Album A");
  TestParser(
      true,
      SIMPLE_HEADER()
      SIMPLE_TRACK(1, 1, "C:/dir/SongA1.mp3", "Artist A", "Album A")
      SIMPLE_TRACK(2, 3, "C:/dir/SongA2.mp3", "Artist A", "Album A")
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, OtherDictionaryEntries) {
  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Album A");
  TestParser(
      true,
      "<plist>"
      "  <dict>"
      "    <key>Other section</key>"
      "    <dict>"
      // In Other section, not Tracks.
      SIMPLE_TRACK(10, 10, "C:/dir/SongB2.mp3", "Artist B", "Album B")
      "    </dict>"
      "    <key>Tracks</key>"
      "    <dict>"
      "      <key>1</key>"
      "      <dict>"
      // In the body of a track dictionary before the interesting entries.
      SIMPLE_TRACK(20, 20, "C:/dir/SongB2.mp3", "Artist B", "Album B")
      // Entries in a different order.
      "        <key>Artist</key><string>Artist A</string>"
      "        <key>Location</key>"
      "          <string>file://localhost/C:/dir/SongA1.mp3</string>"
      "        <key>Album</key><string>Album A</string>"
      "        <key>Track ID</key><integer>1</integer>"
      // In the body of a track dictionary after the interesting entries.
      SIMPLE_TRACK(30, 30, "C:/dir/SongB3.mp3", "Artist B", "Album B")
      "      </dict>"
      "      <key>40</key>"
      "      <dict>"
      // Missing album name.
      "        <key>Artist</key><string>Artist B</string>"
      "        <key>Location</key>"
      "          <string>file://localhost/C:/dir/SongB4.mp3</string>"
      "        <key>Track ID</key><integer>1</integer>"
      "      </dict>"
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, MissingEntry) {
  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Album A");
  AddExpectedTrack(3, "C:/dir/SongA3.mp3", "Artist A", "Album A");
  TestParser(
      true,
      SIMPLE_HEADER()
      SIMPLE_TRACK(1, 1, "C:/dir/SongA1.mp3", "Artist A", "Album A")
      "<key>2</key><dict>"
      "  <key>Track ID</key><integer>2</integer>"
      "  <key>Album</key><string>Album A</string>"
      "  <key>Foo</key><string>Bar</string>"
      "   "  // A whitespace node is important for the test.
      "</dict>"
      SIMPLE_TRACK(3, 3, "C:/dir/SongA3.mp3", "Artist A", "Album A")
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, UnknownAlbumOrArtist) {
  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Unknown Album");
  AddExpectedTrack(2, "C:/dir/SongA2.mp3", "Unknown Artist", "Album A");
  AddExpectedTrack(3, "C:/dir/SongA3.mp3", "Unknown Artist", "Unknown Album");
  TestParser(
      true,
      SIMPLE_HEADER()
      "<key>1</key><dict>"
      "  <key>Track ID</key><integer>1</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA1.mp3</string>"
      "  <key>Artist</key><string>Artist A</string>"
      "</dict>"
      "<key>2</key><dict>"
      "  <key>Track ID</key><integer>2</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA2.mp3</string>"
      "  <key>Album</key><string>Album A</string>"
      "</dict>"
      "<key>3</key><dict>"
      "  <key>Track ID</key><integer>3</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA3.mp3</string>"
      "</dict>"
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, AlbumArtist) {
  AddExpectedTrack(1, "C:/dir/SongA1.mp3", "Artist A", "Unknown Album");
  AddExpectedTrack(2, "C:/dir/SongA2.mp3", "Artist A", "Unknown Album");
  AddExpectedTrack(3, "C:/dir/SongA3.mp3", "Artist A", "Unknown Album");
  AddExpectedTrack(4, "C:/dir/SongA4.mp3", "Artist A", "Album");
  TestParser(
      true,
      SIMPLE_HEADER()
      "<key>1</key><dict>"
      "  <key>Track ID</key><integer>1</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA1.mp3</string>"
      "  <key>Album Artist</key><string>Artist A</string>"
      "</dict>"
      "<key>2</key><dict>"
      "  <key>Track ID</key><integer>2</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA2.mp3</string>"
      "  <key>Artist</key><string>Artist B</string>"
      "  <key>Album Artist</key><string>Artist A</string>"
      "</dict>"
      "<key>3</key><dict>"
      "  <key>Track ID</key><integer>3</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA3.mp3</string>"
      "  <key>Album Artist</key><string>Artist A</string>"
      "  <key>Artist</key><string>Artist B</string>"
      "</dict>"
      "<key>4</key><dict>"
      "  <key>Track ID</key><integer>4</integer>"
      "  <key>Location</key><string>file://localhost/C:/dir/SongA4.mp3</string>"
      "  <key>Album</key><string>Album</string>"
      "  <key>Artist</key><string>Artist B</string>"
      "  <key>Album Artist</key><string>Artist A</string>"
      "</dict>"
      SIMPLE_FOOTER());
}

TEST_F(ITunesLibraryParserTest, MacPath) {
  AddExpectedTrack(1, "dir/Song With Space.mp3", "Artist A", "Album A");
  TestParser(
      true,
      SIMPLE_HEADER()
      // This path is concatenated with "http://localhost/", so no leading
      // slash should be used.
      SIMPLE_TRACK(1, 1, "dir/Song%20With%20Space.mp3", "Artist A", "Album A")
      SIMPLE_FOOTER());
}

}  // namespace

}  // namespace itunes
