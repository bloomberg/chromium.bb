// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using base::FilePath;

namespace iphoto {

class TestIPhotoDataProvider : public IPhotoDataProvider {
 public:
  TestIPhotoDataProvider(const base::FilePath& xml_library_path,
                         const base::Closure& callback)
      : IPhotoDataProvider(xml_library_path),
        callback_(callback) {
  }
  virtual ~TestIPhotoDataProvider() {}

 private:
  virtual void OnLibraryChanged(const base::FilePath& path,
                                bool error) OVERRIDE {
    IPhotoDataProvider::OnLibraryChanged(path, error);
    callback_.Run();
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TestIPhotoDataProvider);
};

class IPhotoDataProviderTest : public InProcessBrowserTest {
 public:
  IPhotoDataProviderTest() {}
  virtual ~IPhotoDataProviderTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(library_dir_.CreateUniqueTempDir());
    WriteLibraryInternal();
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
        base::Bind(&IPhotoDataProviderTest::StartTestOnMediaTaskRunner,
                   base::Unretained(this)));
    loop.Run();
  }

  void WriteLibrary(const base::Closure& callback) {
    SetLibraryChangeCallback(callback);
    WriteLibraryInternal();
  }

  void SetLibraryChangeCallback(const base::Closure& callback) {
    EXPECT_TRUE(library_changed_callback_.is_null());
    library_changed_callback_ = callback;
  }

  IPhotoDataProvider* data_provider() const {
    return ImportedMediaGalleryRegistry::IPhotoDataProvider();
  }

  const base::FilePath& library_dir() const {
    return library_dir_.path();
  }

  base::FilePath XmlFile() const {
    return library_dir_.path().AppendASCII("library.xml");
  }

  // Start the test. The data provider is refreshed before calling StartTest
  // and the result of the refresh is passed in.
  virtual void StartTest(bool parse_success) = 0;

  void TestDone() {
    DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
    ImportedMediaGalleryRegistry* imported_registry =
        ImportedMediaGalleryRegistry::GetInstance();
    imported_registry->iphoto_data_provider_.reset();
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                     quit_closure_);
  }

  // Override to provide a full library string.
  virtual std::string GetLibraryString() {
    return "<plist><dict>\n</dict></plist>\n";
  }

 private:
  void StartTestOnMediaTaskRunner() {
    DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
    ImportedMediaGalleryRegistry* imported_registry =
        ImportedMediaGalleryRegistry::GetInstance();
    imported_registry->iphoto_data_provider_.reset(
        new TestIPhotoDataProvider(
            XmlFile(),
            base::Bind(&IPhotoDataProviderTest::OnLibraryChanged,
                       base::Unretained(this))));
    data_provider()->RefreshData(base::Bind(&IPhotoDataProviderTest::StartTest,
                                            base::Unretained(this)));
  };

  void OnLibraryChanged() {
    DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
    if (!library_changed_callback_.is_null()) {
      library_changed_callback_.Run();
      library_changed_callback_.Reset();
    }
  }

  void WriteLibraryInternal() {
    std::string xml = GetLibraryString();
    ASSERT_EQ(static_cast<int>(xml.size()),
              base::WriteFile(XmlFile(), xml.c_str(), xml.size()));
  }

  base::ScopedTempDir library_dir_;

  base::Closure library_changed_callback_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(IPhotoDataProviderTest);
};

class IPhotoDataProviderBasicTest : public IPhotoDataProviderTest {
 public:
  IPhotoDataProviderBasicTest() {}
  virtual ~IPhotoDataProviderBasicTest() {}

  virtual std::string GetLibraryString() OVERRIDE {
    return "<plist><dict>\n"
      "<key>List of Albums</key>\n"
      "<array>"
      "    <dict>\n"
      "      <key>AlbumId</key>"
      "      <integer>14</integer>"
      "      <key>AlbumName</key>"
      "      <string>Album1</string>"
      "      <key>KeyList</key>"
      "      <array>"
      "      <string>1</string>"
      "      <string>3</string>"  // [3] and [4] are name dupes
      "      <string>4</string>"
      "      </array>"
      "    </dict>\n"
      "    <dict>\n"
      "      <key>AlbumId</key>"
      "      <integer>15</integer>"
      "      <key>AlbumName</key>"
      "      <string>Album2</string>"
      "      <key>KeyList</key>"
      "      <array>"
      "      <string>2</string>"
      "      </array>"
      "    </dict>\n"
      "    <dict>\n"
      "      <key>AlbumId</key>"
      "      <integer>16</integer>"
      "      <key>AlbumName</key>"
      "      <string>Album5</string>"
      "      <key>KeyList</key>"
      "      <array>"
      "      <string>5</string>"  // A name dupe of [2], but in another album.
      "      </array>"
      "    </dict>\n"
      "</array>\n"
      "<key>Master Image List</key>\n"
      "<dict>\n"
      "  <key>1</key>\n"
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption</key>"
      "    <string>caption</string>"
      "    <key>GUID</key>\n"
      "    <string>guid1</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/path1.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/thumb1.jpg</string>"
      "  </dict>\n"
      "  <key>2</key>\n"
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption</key>"
      "    <string>caption2</string>"
      "    <key>GUID</key>\n"
      "    <string>guid2</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/path2.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/thumb2.jpg</string>"
      "  </dict>\n"
      "  <key>3</key>\n"
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption</key>"
      "    <string>caption3</string>"
      "    <key>GUID</key>\n"
      "    <string>guid3</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/path3.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/thumb3.jpg</string>"
      "  </dict>\n"
      "  <key>4</key>\n"  // A name duplicate of [3] in another path.
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption</key>"
      "    <string>caption</string>"
      "    <key>GUID</key>\n"
      "    <string>guid3</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/dupe/path3.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/dupe/thumb3.jpg</string>"
      "  </dict>\n"
      "  <key>5</key>\n"  // A name duplicate of [2] in another path.
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption</key>"
      "    <string>caption5</string>"
      "    <key>GUID</key>\n"
      "    <string>guid2</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/dupe/path2.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/dupe/thumb2.jpg</string>"
      "    <key>OriginalPath</key>"             \
      "    <string>/original/vol/another2.jpg</string>"  \
      "  </dict>\n"
      "</dict>\n"
      "</dict></plist>\n";
  }

  virtual void StartTest(bool parse_success) OVERRIDE {
    EXPECT_TRUE(parse_success);

    std::vector<std::string> names = data_provider()->GetAlbumNames();
    EXPECT_EQ(3U, names.size());
    EXPECT_EQ("Album1", names[0]);

    EXPECT_EQ(FilePath("/vol/path1.jpg").value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album1", "path1.jpg").value());
    EXPECT_EQ(FilePath("/vol/path3.jpg").value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album1", "path3.jpg").value());
    EXPECT_EQ(FilePath("/vol/dupe/path3.jpg").value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album1", "path3(4).jpg").value());
    EXPECT_EQ(FilePath().value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album1", "path5.jpg").value());

    // path2.jpg is name-duped, but in different albums, and so should not
    // be mangled.
    EXPECT_EQ(FilePath("/vol/dupe/path2.jpg").value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album5", "path2.jpg").value());
    EXPECT_EQ(FilePath("/vol/path2.jpg").value(),
              data_provider()->GetPhotoLocationInAlbum(
                  "Album2", "path2.jpg").value());

    std::map<std::string, base::FilePath> photos =
        data_provider()->GetAlbumContents("nonexistent");
    EXPECT_EQ(0U, photos.size());
    photos = data_provider()->GetAlbumContents("Album1");
    EXPECT_EQ(3U, photos.size());
    EXPECT_TRUE(ContainsKey(photos, "path1.jpg"));
    EXPECT_FALSE(ContainsKey(photos, "path2.jpg"));
    EXPECT_TRUE(ContainsKey(photos, "path3.jpg"));
    EXPECT_TRUE(ContainsKey(photos, "path3(4).jpg"));
    EXPECT_EQ(FilePath("/vol/path1.jpg").value(), photos["path1.jpg"].value());
    EXPECT_EQ(FilePath("/vol/path3.jpg").value(),
        photos["path3.jpg"].value());
    EXPECT_EQ(FilePath("/vol/dupe/path3.jpg").value(),
        photos["path3(4).jpg"].value());

    photos = data_provider()->GetAlbumContents("Album2");
    EXPECT_EQ(1U, photos.size());
    EXPECT_TRUE(ContainsKey(photos, "path2.jpg"));

    EXPECT_FALSE(data_provider()->HasOriginals("Album1"));
    EXPECT_TRUE(data_provider()->HasOriginals("Album5"));
    std::map<std::string, base::FilePath> originals =
        data_provider()->GetOriginals("Album1");
    EXPECT_EQ(0U, originals.size());
    originals = data_provider()->GetOriginals("Album5");
    EXPECT_EQ(1U, originals.size());
    EXPECT_TRUE(ContainsKey(originals, "path2.jpg"));
    EXPECT_FALSE(ContainsKey(originals, "path1.jpg"));
    EXPECT_EQ(FilePath("/original/vol/another2.jpg").value(),
              originals["path2.jpg"].value());
    base::FilePath original_path =
        data_provider()->GetOriginalPhotoLocation("Album5", "path2.jpg");
    EXPECT_EQ(FilePath("/original/vol/another2.jpg").value(),
              original_path.value());

    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IPhotoDataProviderBasicTest);
};

class IPhotoDataProviderRefreshTest : public IPhotoDataProviderTest {
 public:
  IPhotoDataProviderRefreshTest() {}
  virtual ~IPhotoDataProviderRefreshTest() {}

  std::string another_album;

  virtual std::string GetLibraryString() OVERRIDE {
    return "<plist><dict>\n"
      "<key>List of Albums</key>\n"
      "<array>"
      "    <dict>"
      "      <key>AlbumId</key>"
      "      <integer>14</integer>"
      "      <key>AlbumName</key>"
      "      <string>Album1</string>"
      "      <key>KeyList</key>"
      "      <array>"
      "      <string>1</string>"
      "      </array>"
      "    </dict>\n" +
      another_album +
      "</array>\n"
      "<key>Master Image List</key>\n"
      "<dict>\n"
      "  <key>1</key>\n"
      "  <dict>\n"
      "    <key>MediaType</key>"
      "    <string>Image</string>"
      "    <key>Caption1</key>"
      "    <string>caption</string>"
      "    <key>GUID</key>\n"
      "    <string>guid1</string>"
      "    <key>ImagePath</key>"
      "    <string>/vol/path1.jpg</string>"
      "    <key>ThumbPath</key>"
      "    <string>/vol/thumb1.jpg</string>"
      "  </dict>\n"
      "</dict>\n"
      "</dict></plist>\n";
  }

  virtual void StartTest(bool parse_success) OVERRIDE {
    EXPECT_TRUE(parse_success);

    EXPECT_EQ(FilePath("/vol/path1.jpg"),
              data_provider()->GetPhotoLocationInAlbum("Album1", "path1.jpg"));
    std::vector<std::string> names = data_provider()->GetAlbumNames();
    EXPECT_EQ(1U, names.size());
    EXPECT_EQ("Album1", names[0]);

    another_album =
      "    <dict>"
      "      <key>AlbumId</key>"
      "      <integer>14</integer>"
      "      <key>AlbumName</key>"
      "      <string>Another Album</string>"
      "      <key>KeyList</key>"
      "      <array>"
      "      <string>1</string>"
      "      </array>"
      "    </dict>\n";

    WriteLibrary(base::Bind(&IPhotoDataProviderRefreshTest::CheckAfterWrite,
                            base::Unretained(this)));
  }

  void CheckAfterWrite() {
    // No change -- data has not been parsed.
    EXPECT_EQ(FilePath("/vol/path1.jpg"),
              data_provider()->GetPhotoLocationInAlbum("Album1", "path1.jpg"));
    std::vector<std::string> names = data_provider()->GetAlbumNames();
    EXPECT_EQ(1U, names.size());
    EXPECT_EQ("Album1", names[0]);

    data_provider()->RefreshData(
        base::Bind(&IPhotoDataProviderRefreshTest::CheckRefresh,
                   base::Unretained(this)));
  }

  void CheckRefresh(bool is_valid) {
    EXPECT_TRUE(is_valid);

    EXPECT_EQ(FilePath("/vol/path1.jpg"),
              data_provider()->GetPhotoLocationInAlbum("Album1", "path1.jpg"));
    std::vector<std::string> names = data_provider()->GetAlbumNames();
    EXPECT_EQ(2U, names.size());
    if (names.size() == 2U) {
      EXPECT_EQ("Album1", names[0]);
      EXPECT_EQ("Another Album", names[1]);
    }

    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IPhotoDataProviderRefreshTest);
};

class IPhotoDataProviderInvalidTest : public IPhotoDataProviderTest {
 public:
  IPhotoDataProviderInvalidTest() {}
  virtual ~IPhotoDataProviderInvalidTest() {}

  virtual void StartTest(bool parse_success) OVERRIDE {
    EXPECT_TRUE(parse_success);

    SetLibraryChangeCallback(
        base::Bind(&IPhotoDataProvider::RefreshData,
                   base::Unretained(data_provider()),
                   base::Bind(&IPhotoDataProviderInvalidTest::CheckInvalid,
                              base::Unretained(this))));
    EXPECT_EQ(1L, base::WriteFile(XmlFile(), " ", 1));
  }

  void CheckInvalid(bool is_valid) {
    EXPECT_FALSE(is_valid);
    TestDone();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IPhotoDataProviderInvalidTest);
};

IN_PROC_BROWSER_TEST_F(IPhotoDataProviderBasicTest, BasicTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(IPhotoDataProviderRefreshTest, RefreshTest) {
  RunTest();
}

IN_PROC_BROWSER_TEST_F(IPhotoDataProviderInvalidTest, InvalidTest) {
  RunTest();
}

}  // namespace iphoto
