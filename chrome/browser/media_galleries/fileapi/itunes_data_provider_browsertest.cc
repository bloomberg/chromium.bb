// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

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

// 'c' with combinding cedilla.
const char kDeNormalizedName[] = {
    'c', static_cast<unsigned char>(0xCC), static_cast<unsigned char>(0xA7), 0};
// 'c' with cedilla.
const char kNormalizedName[] = {
    static_cast<unsigned char>(0xC3), static_cast<unsigned char>(0xA7), 0};

}  // namespace

class TestITunesDataProvider : public ITunesDataProvider {
 public:
  TestITunesDataProvider(const base::FilePath& xml_library_path,
                         const base::Closure& callback)
      : ITunesDataProvider(xml_library_path),
        callback_(callback) {
  }
  ~TestITunesDataProvider() override {}

 private:
  void OnLibraryChanged(const base::FilePath& path, bool error) override {
    ITunesDataProvider::OnLibraryChanged(path, error);
    callback_.Run();
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TestITunesDataProvider);
};

class ITunesDataProviderTest : public InProcessBrowserTest {
 public:
  ITunesDataProviderTest() {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(library_dir_.CreateUniqueTempDir());
    WriteLibraryInternal(SetUpLibrary());
    // The ImportedMediaGalleryRegistry is created on which ever thread calls
    // GetInstance() first.  It shouldn't matter what thread creates, however
    // in practice it is always created on the UI thread, so this calls
    // GetInstance here to mirror those real conditions.
    ImportedMediaGalleryRegistry::GetInstance();
    InProcessBrowserTest::SetUp();
  }

  void RunTest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ITunesDataProviderTest::StartTestOnMediaTaskRunner,
                   base::Unretained(this)));
    loop.Run();
  }

  void WriteLibrary(const std::vector<LibraryEntry>& entries,
                    const base::Closure& callback) {
    SetLibraryChangeCallback(callback);
    WriteLibraryInternal(entries);
  }

  void SetLibraryChangeCallback(const base::Closure& callback) {
    EXPECT_TRUE(library_changed_callback_.is_null());
    library_changed_callback_ = callback;
  }

  ITunesDataProvider* data_provider() const {
    return ImportedMediaGalleryRegistry::ITunesDataProvider();
  }

  const base::FilePath& library_dir() const { return library_dir_.GetPath(); }

  base::FilePath XmlFile() const {
    return library_dir_.GetPath().AppendASCII("library.xml");
  }

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


  // Get the initial set of library entries, called by SetUp.  If no entries
  // are returned the xml file is not created.
  virtual std::vector<LibraryEntry> SetUpLibrary() {
    return std::vector<LibraryEntry>();
  }

  // Start the test. The data provider is refreshed before calling StartTest
  // and the result of the refresh is passed in.
  virtual void StartTest(bool parse_success) = 0;

  void TestDone() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
    ImportedMediaGalleryRegistry* imported_registry =
        ImportedMediaGalleryRegistry::GetInstance();
    imported_registry->itunes_data_provider_.reset();
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     quit_closure_);
  }

 private:
  void StartTestOnMediaTaskRunner() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
    ImportedMediaGalleryRegistry* imported_registry =
        ImportedMediaGalleryRegistry::GetInstance();
    imported_registry->itunes_data_provider_.reset(
        new TestITunesDataProvider(
            XmlFile(),
            base::Bind(&ITunesDataProviderTest::OnLibraryChanged,
                       base::Unretained(this))));
    data_provider()->RefreshData(base::Bind(&ITunesDataProviderTest::StartTest,
                                            base::Unretained(this)));
  }

  void OnLibraryChanged() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
    if (!library_changed_callback_.is_null()) {
      library_changed_callback_.Run();
      library_changed_callback_.Reset();
    }
  }

  void WriteLibraryInternal(const std::vector<LibraryEntry>& entries) {
    if (entries.empty())
      return;
    std::string xml = "<plist><dict><key>Tracks</key><dict>\n";
    for (size_t i = 0; i < entries.size(); ++i) {
      std::string separator;
#if defined(OS_WIN)
      separator = "/";
#endif
      GURL location("file://localhost" + separator +
                    entries[i].location.AsUTF8Unsafe());
      std::string entry_string = base::StringPrintf(
          "<key>%" PRIuS "</key><dict>\n"
          "  <key>Track ID</key><integer>%" PRIuS "</integer>\n"
          "  <key>Location</key><string>%s</string>\n"
          "  <key>Artist</key><string>%s</string>\n"
          "  <key>Album</key><string>%s</string>\n"
          "</dict>\n",
          i + 1, i + 1, location.spec().c_str(), entries[i].artist.c_str(),
          entries[i].album.c_str());
      xml += entry_string;
    }
    xml += "</dict></dict></plist>\n";
    ASSERT_EQ(static_cast<int>(xml.size()),
              base::WriteFile(XmlFile(), xml.c_str(), xml.size()));
  }

  base::ScopedTempDir library_dir_;

  base::Closure library_changed_callback_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderTest);
};

class ITunesDataProviderBasicTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderBasicTest() {}

  std::vector<LibraryEntry> SetUpLibrary() override {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist", "Album", track));
    return entries;
  }

  void StartTest(bool parse_success) override {
    EXPECT_TRUE(parse_success);

    // KnownArtist
    EXPECT_TRUE(data_provider()->KnownArtist("Artist"));
    EXPECT_FALSE(data_provider()->KnownArtist("Artist2"));

    // KnownAlbum
    EXPECT_TRUE(data_provider()->KnownAlbum("Artist", "Album"));
    EXPECT_FALSE(data_provider()->KnownAlbum("Artist", "Album2"));
    EXPECT_FALSE(data_provider()->KnownAlbum("Artist2", "Album"));

    // GetTrackLocation
    ExpectTrackLocation("Artist", "Album", "Track.mp3");
    ExpectNoTrack("Artist", "Album", "Track2.mp3");
    ExpectNoTrack("Artist", "Album2", "Track.mp3");
    ExpectNoTrack("Artist2", "Album", "Track.mp3");

    // GetArtistNames
    std::set<ITunesDataProvider::ArtistName> artists =
      data_provider()->GetArtistNames();
    ASSERT_EQ(1U, artists.size());
    EXPECT_EQ("Artist", *artists.begin());

    // GetAlbumNames
    std::set<ITunesDataProvider::AlbumName> albums =
        data_provider()->GetAlbumNames("Artist");
    ASSERT_EQ(1U, albums.size());
    EXPECT_EQ("Album", *albums.begin());

    albums = data_provider()->GetAlbumNames("Artist2");
    EXPECT_EQ(0U, albums.size());

    // GetAlbum
    base::FilePath track =
        library_dir().AppendASCII("Track.mp3").NormalizePathSeparators();
    ITunesDataProvider::Album album =
        data_provider()->GetAlbum("Artist", "Album");
    ASSERT_EQ(1U, album.size());
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

  std::vector<LibraryEntry> SetUpLibrary() override {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist", "Album", track));
    return entries;
  }

  void StartTest(bool parse_success) override {
    EXPECT_TRUE(parse_success);

    // Initial contents.
    ExpectTrackLocation("Artist", "Album", "Track.mp3");
    ExpectNoTrack("Artist2", "Album2", "Track2.mp3");

    // New file.
    base::FilePath track2 = library_dir().AppendASCII("Track2.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist2", "Album2", track2));
    WriteLibrary(entries,
                 base::Bind(&ITunesDataProviderRefreshTest::CheckAfterWrite,
                            base::Unretained(this)));
  }

  void CheckAfterWrite() {
    // Content the same.
    ExpectTrackLocation("Artist", "Album", "Track.mp3");
    ExpectNoTrack("Artist2", "Album2", "Track2.mp3");

    data_provider()->RefreshData(
        base::Bind(&ITunesDataProviderRefreshTest::CheckRefresh,
                   base::Unretained(this)));
  }

  void CheckRefresh(bool is_valid) {
    EXPECT_TRUE(is_valid);

    ExpectTrackLocation("Artist2", "Album2", "Track2.mp3");
    ExpectNoTrack("Artist", "Album", "Track.mp3");
    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderRefreshTest);
};

class ITunesDataProviderInvalidTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderInvalidTest() {}

  std::vector<LibraryEntry> SetUpLibrary() override {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist", "Album", track));
    return entries;
  }

  void StartTest(bool parse_success) override {
    EXPECT_TRUE(parse_success);

    SetLibraryChangeCallback(
        base::Bind(&ITunesDataProvider::RefreshData,
                   base::Unretained(data_provider()),
                   base::Bind(&ITunesDataProviderInvalidTest::CheckInvalid,
                              base::Unretained(this))));
    ASSERT_EQ(1L, base::WriteFile(XmlFile(), " ", 1));
  }

  void CheckInvalid(bool is_valid) {
    EXPECT_FALSE(is_valid);
    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderInvalidTest);
};

class ITunesDataProviderUniqueNameTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderUniqueNameTest() {}

  std::vector<LibraryEntry> SetUpLibrary() override {
    base::FilePath track = library_dir().AppendASCII("Track.mp3");
    std::vector<LibraryEntry> entries;
    // Dupe album names should get uniquified with the track id, which in the
    // test framework is the vector index.
    entries.push_back(LibraryEntry("Artist", "Album", track));
    entries.push_back(LibraryEntry("Artist", "Album", track));
    entries.push_back(LibraryEntry("Artist", "Album2", track));
    return entries;
  }

  void StartTest(bool parse_success) override {
    EXPECT_TRUE(parse_success);

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

// Albums and tracks that aren't the same, but become the same after
// replacing bad characters are not handled properly, but that case should
// never happen in practice.
class ITunesDataProviderEscapeTest : public ITunesDataProviderTest {
 public:
  ITunesDataProviderEscapeTest() {}

  std::vector<LibraryEntry> SetUpLibrary() override {
    base::FilePath track = library_dir().AppendASCII("Track:1.mp3");
    std::vector<LibraryEntry> entries;
    entries.push_back(LibraryEntry("Artist:/name", "Album:name/", track));
    entries.push_back(LibraryEntry("Artist/name", "Album:name", track));
    entries.push_back(LibraryEntry("Artist/name", "Album:name", track));
    entries.push_back(LibraryEntry(kDeNormalizedName, kNormalizedName, track));
    return entries;
  }

  void StartTest(bool parse_success) override {
    EXPECT_TRUE(parse_success);

    base::FilePath track =
        library_dir().AppendASCII("Track:1.mp3").NormalizePathSeparators();
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist__name", "Album_name_",
                  "Track_1.mp3").NormalizePathSeparators().value());
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist_name", "Album_name",
                  "Track_1 (2).mp3").NormalizePathSeparators().value());
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  "Artist_name", "Album_name",
                  "Track_1 (3).mp3").NormalizePathSeparators().value());
    EXPECT_EQ(track.value(),
              data_provider()->GetTrackLocation(
                  kNormalizedName, kNormalizedName,
                  "Track_1.mp3").NormalizePathSeparators().value());

    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ITunesDataProviderEscapeTest);
};

IN_PROC_BROWSER_TEST_F(ITunesDataProviderBasicTest, BasicTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderRefreshTest, RefreshTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderInvalidTest, InvalidTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderUniqueNameTest, UniqueNameTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(ITunesDataProviderEscapeTest, EscapeTest) {
  RunTest();
}

}  // namespace itunes
