// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

using chrome::MediaFileSystemMountPointProvider;

namespace itunes {

namespace {

struct LibraryEntry {
  LibraryEntry(const std::string& artist, const std::string& album,
               const base::FilePath& location)
      : artist(artist),
        album(album),
        location(location) {
  }
  std::string artist;
  std::string album;
  base::FilePath location;
};

}  // namespace

class ITunesDataProviderTest : public InProcessBrowserTest {
 public:
  ITunesDataProviderTest() {}
  virtual ~ITunesDataProviderTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(library_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void RunTestOnMediaTaskRunner() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    chrome::ImportedMediaGalleryRegistry* imported_registry =
        chrome::ImportedMediaGalleryRegistry::GetInstance();
    std::string itunes_fsid =
        imported_registry->RegisterITunesFilesystemOnUIThread(XmlFile());

    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
        FROM_HERE, base::Bind(&ITunesDataProviderTest::StartTest,
                              base::Unretained(this)));
    loop.Run();

    imported_registry->RevokeImportedFilesystemOnUIThread(itunes_fsid);
  }

  void WriteLibrary(const std::vector<LibraryEntry>& entries) {
    std::string xml = "<plist><dict><key>Tracks</key><dict>\n";
    for (size_t i = 0; i < entries.size(); ++i) {
      GURL location("file://localhost/" + entries[i].location.AsUTF8Unsafe());
      // Visual studio doesn't like %zd, so cast to int instead.
      int id = static_cast<int>(i) + 1;
      std::string entry_string = base::StringPrintf(
          "<key>%d</key><dict>\n"
          "  <key>Track ID</key><integer>%d</integer>\n"
          "  <key>Location</key><string>%s</string>\n"
          "  <key>Album Artist</key><string>%s</string>\n"
          "  <key>Album</key><string>%s</string>\n"
          "</dict>\n",
          id, id, location.spec().c_str(), entries[i].artist.c_str(),
          entries[i].album.c_str());
      xml += entry_string;
    }
    xml += "</dict></dict></plist>\n";
    file_util::WriteFile(XmlFile(), xml.c_str(), xml.size());
  }

  void RefreshData(const ITunesDataProvider::ReadyCallback& callback) {
    data_provider()->RefreshData(callback);
  }

  ITunesDataProvider* data_provider() {
    return chrome::ImportedMediaGalleryRegistry::ITunesDataProvider();
  }

  base::FilePath library_dir() {
    return library_dir_.path();
  }

  base::FilePath XmlFile() {
    return library_dir_.path().AppendASCII("library.xml");
  }

 protected:
  virtual void StartTest() = 0;

  void TestDone() {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     quit_closure_);
  }

 private:
  base::ScopedTempDir library_dir_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderTest);
};

class ITunesDataProviderBasicTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderBasicTest() {}
  virtual ~ITunesDataProviderBasicTest() {}

  virtual void StartTest() OVERRIDE {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist", "Album", track));

    WriteLibrary(entries);
    RefreshData(base::Bind(&ITunesDataProviderBasicTest::CheckData,
                           base::Unretained(this)));
  }

  void CheckData(bool is_valid) {
    EXPECT_TRUE(is_valid);

    // KnownArtist
    EXPECT_TRUE(data_provider()->KnownArtist("Artist"));
    EXPECT_FALSE(data_provider()->KnownArtist("Artist2"));

    // KnownAlbum
    EXPECT_TRUE(data_provider()->KnownAlbum("Artist", "Album"));
    EXPECT_FALSE(data_provider()->KnownAlbum("Artist", "Album2"));
    EXPECT_FALSE(data_provider()->KnownAlbum("Artist2", "Album"));

    // GetTrackLocation
    base::FilePath track =
        library_dir().AppendASCII("Track.mp3").NormalizePathSeparators();
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist", "Album",
                  "Track.mp3").NormalizePathSeparators().value());
    EXPECT_TRUE(data_provider()->GetTrackLocation("Artist", "Album",
                                                  "Track2.mp3").empty());
    EXPECT_TRUE(data_provider()->GetTrackLocation("Artist", "Album2",
                                                  "Track.mp3").empty());
    EXPECT_TRUE(data_provider()->GetTrackLocation("Artist2", "Album",
                                                  "Track.mp3").empty());

    // GetArtistNames
    std::set<ITunesDataProvider::ArtistName> artists =
      data_provider()->GetArtistNames();
    EXPECT_EQ(1U, artists.size());
    EXPECT_EQ(std::string("Artist"), *artists.begin());

    // GetAlbumNames
    std::set<ITunesDataProvider::AlbumName> albums =
        data_provider()->GetAlbumNames("Artist");
    EXPECT_EQ(1U, albums.size());
    EXPECT_EQ(std::string("Album"), *albums.begin());

    albums = data_provider()->GetAlbumNames("Artist2");
    EXPECT_EQ(0U, albums.size());

    // GetAlbum
    ITunesDataProvider::Album album =
        data_provider()->GetAlbum("Artist", "Album");
    EXPECT_EQ(1U, album.size());
    EXPECT_EQ(track.BaseName().AsUTF8Unsafe(), album.begin()->first);
    EXPECT_EQ(track.value(),
              album.begin()->second.NormalizePathSeparators().value());

    album = data_provider()->GetAlbum("Artist", "Album2");
    EXPECT_EQ(0U, album.size());

    album = data_provider()->GetAlbum("Artist2", "Album");
    EXPECT_EQ(0U, album.size());

    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderBasicTest);
};

class ITunesDataProviderRefreshTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderRefreshTest() {}
  virtual ~ITunesDataProviderRefreshTest() {}

  virtual void StartTest() OVERRIDE {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist", "Album", track));

    WriteLibrary(entries);
    RefreshData(base::Bind(&ITunesDataProviderRefreshTest::CheckData,
                           base::Unretained(this)));
  }

  void CheckData(bool is_valid) {
    EXPECT_TRUE(is_valid);

    // Initial contents.
    ExpectTrackLocation("Artist", "Album", "Track.mp3");
    ExpectNoTrack("Artist2", "Album2", "Track2.mp3");

    // New file.
    base::FilePath track2 = library_dir().AppendASCII("Track2.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist2", "Album2", track2));
    WriteLibrary(entries);

    // Content the same.
    ExpectTrackLocation("Artist", "Album", "Track.mp3");
    ExpectNoTrack("Artist2", "Album2", "Track2.mp3");

    RefreshData(base::Bind(&ITunesDataProviderRefreshTest::CheckRefresh,
                           base::Unretained(this)));
  }

  void CheckRefresh(bool is_valid) {
    EXPECT_TRUE(is_valid);

    ExpectTrackLocation("Artist2", "Album2", "Track2.mp3");
    ExpectNoTrack("Artist", "Album", "Track.mp3");

    file_util::WriteFile(XmlFile(), " ", 1);

    ExpectTrackLocation("Artist2", "Album2", "Track2.mp3");
    ExpectNoTrack("Artist", "Album", "Track.mp3");

    RefreshData(base::Bind(&ITunesDataProviderRefreshTest::CheckInvalid,
                           base::Unretained(this)));
  }

  void CheckInvalid(bool is_valid) {
    EXPECT_FALSE(is_valid);
    TestDone();
  }

 private:
  void ExpectTrackLocation(const std::string& artist, const std::string& album,
                           const std::string& track_name) {
    base::FilePath track =
        library_dir().AppendASCII(track_name).NormalizePathSeparators();
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  artist, album, track_name).NormalizePathSeparators().value());
  }

  void ExpectNoTrack(const std::string& artist, const std::string& album,
                     const std::string& track_name) {
    EXPECT_TRUE(data_provider()->GetTrackLocation(
          artist, album, track_name).empty()) << track_name;
  }

  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderRefreshTest);
};

class ITunesDataProviderUniqueNameTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderUniqueNameTest() {}
  virtual ~ITunesDataProviderUniqueNameTest() {}

  virtual void StartTest() OVERRIDE {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    // Dupe album names should get uniquified with the track id, which in the
    // test framework is the vector index.
    entries.push_back(LibraryEntry("Artist", "Album", track));
    entries.push_back(LibraryEntry("Artist", "Album", track));
    entries.push_back(LibraryEntry("Artist", "Album2", track));

    WriteLibrary(entries);
    RefreshData(base::Bind(&ITunesDataProviderUniqueNameTest::CheckData,
                           base::Unretained(this)));
  }

  void CheckData(bool is_valid) {
    EXPECT_TRUE(is_valid);

    base::FilePath track =
        library_dir().AppendASCII("Track.mp3").NormalizePathSeparators();
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist", "Album",
                  "Track (1).mp3").NormalizePathSeparators().value());
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist", "Album",
                  "Track (2).mp3").NormalizePathSeparators().value());
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist", "Album2",
                  "Track.mp3").NormalizePathSeparators().value());

    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderUniqueNameTest);
};

IN_PROC_BROWSER_TEST_F(ITunesDataProviderBasicTest, BasicTest) {
  RunTestOnMediaTaskRunner();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderRefreshTest, RefreshTest) {
  RunTestOnMediaTaskRunner();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderUniqueNameTest, UniqueNameTest) {
  RunTestOnMediaTaskRunner();
}

}  // namespace itunes
