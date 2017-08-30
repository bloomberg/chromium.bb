// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa_data_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/media_galleries/picasa_test_util.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_browser_thread.h"

namespace picasa {

namespace {

void VerifyTestAlbumTable(PicasaDataProvider* data_provider,
                          base::FilePath test_folder_1_path,
                          base::FilePath test_folder_2_path) {
  std::unique_ptr<AlbumMap> folders = data_provider->GetFolders();
  ASSERT_TRUE(folders.get());
  EXPECT_EQ(2u, folders->size());

  AlbumMap::const_iterator folder_1 = folders->find(
      test_folder_1_path.BaseName().AsUTF8Unsafe() + " 1899-12-30");
  EXPECT_NE(folders->end(), folder_1);
  EXPECT_EQ(test_folder_1_path.BaseName().AsUTF8Unsafe(),
            folder_1->second.name);
  EXPECT_EQ(test_folder_1_path, folder_1->second.path);
  EXPECT_EQ("uid1", folder_1->second.uid);

  AlbumMap::const_iterator folder_2 = folders->find(
      test_folder_2_path.BaseName().AsUTF8Unsafe() + " 1899-12-30");
  EXPECT_NE(folders->end(), folder_2);
  EXPECT_EQ(test_folder_2_path.BaseName().AsUTF8Unsafe(),
            folder_2->second.name);
  EXPECT_EQ(test_folder_2_path, folder_2->second.path);
  EXPECT_EQ("uid4", folder_2->second.uid);

  std::unique_ptr<AlbumMap> albums = data_provider->GetAlbums();
  ASSERT_TRUE(albums.get());
  EXPECT_EQ(2u, albums->size());

  AlbumMap::const_iterator album_1 = albums->find("Album 1 Name 1899-12-30");
  EXPECT_NE(albums->end(), album_1);
  EXPECT_EQ("Album 1 Name", album_1->second.name);
  EXPECT_EQ(base::FilePath(), album_1->second.path);
  EXPECT_EQ("uid3", album_1->second.uid);

  AlbumMap::const_iterator album_2 = albums->find("Album 2 Name 1899-12-30");
  EXPECT_NE(albums->end(), album_2);
  EXPECT_EQ("Album 2 Name", album_2->second.name);
  EXPECT_EQ(base::FilePath(), album_2->second.path);
  EXPECT_EQ("uid5", album_2->second.uid);
}

void VerifyTestAlbumsImagesIndex(PicasaDataProvider* data_provider,
                                 base::FilePath test_folder_1_path,
                                 base::FilePath test_folder_2_path) {
  base::File::Error error;
  std::unique_ptr<AlbumImages> album_1_images =
      data_provider->FindAlbumImages("uid3", &error);
  ASSERT_TRUE(album_1_images);
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(2u, album_1_images->size());
  EXPECT_NE(album_1_images->end(), album_1_images->find("InBoth.jpg"));
  EXPECT_EQ(test_folder_1_path.AppendASCII("InBoth.jpg"),
            (*album_1_images)["InBoth.jpg"]);
  EXPECT_NE(album_1_images->end(),
            album_1_images->find("InFirstAlbumOnly.jpg"));
  EXPECT_EQ(test_folder_2_path.AppendASCII("InFirstAlbumOnly.jpg"),
            (*album_1_images)["InFirstAlbumOnly.jpg"]);

  std::unique_ptr<AlbumImages> album_2_images =
      data_provider->FindAlbumImages("uid5", &error);
  ASSERT_TRUE(album_2_images);
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(2u, album_2_images->size());
  EXPECT_NE(album_2_images->end(), album_2_images->find("InBoth.jpg"));
  EXPECT_EQ(test_folder_1_path.AppendASCII("InBoth.jpg"),
            (*album_2_images)["InBoth.jpg"]);
  EXPECT_NE(album_2_images->end(),
            album_2_images->find("InSecondAlbumOnly.jpg"));
  EXPECT_EQ(test_folder_1_path.AppendASCII("InSecondAlbumOnly.jpg"),
            (*album_2_images)["InSecondAlbumOnly.jpg"]);
}

}  // namespace

class TestPicasaDataProvider : public PicasaDataProvider {
 public:
  explicit TestPicasaDataProvider(const base::FilePath& database_path)
      : PicasaDataProvider(database_path) {}

  ~TestPicasaDataProvider() override {}

  // Simulates the actual writing process of moving all the database files
  // from the temporary directory to the database directory in a loop.
  void MoveTempFilesToDatabase() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    base::FileEnumerator file_enumerator(
        database_path_.DirName().AppendASCII(kPicasaTempDirName),
        false /* recursive */,
        base::FileEnumerator::FILES);

    for (base::FilePath src_path = file_enumerator.Next(); !src_path.empty();
         src_path = file_enumerator.Next()) {
      ASSERT_TRUE(
          base::Move(src_path, database_path_.Append(src_path.BaseName())));
    }
  }

  void SetInvalidateCallback(const base::Closure& callback) {
    DCHECK(invalidate_callback_.is_null());
    invalidate_callback_ = callback;
  }

  void InvalidateData() override {
    PicasaDataProvider::InvalidateData();

    if (!invalidate_callback_.is_null()) {
      invalidate_callback_.Run();
      invalidate_callback_.Reset();
    }
  }

  void SetAlbumMapsForTesting(const AlbumMap& album_map,
                              const AlbumMap& folder_map) {
    album_map_ = album_map;
    folder_map_ = folder_map;
  }

 private:
  base::Closure invalidate_callback_;
};

class PicasaDataProviderTest : public InProcessBrowserTest {
 public:
  PicasaDataProviderTest() {}
  ~PicasaDataProviderTest() override {}

 protected:
  // Runs on the MediaTaskRunner and designed to be overridden by subclasses.
  virtual void InitializeTestData() {}

  void RunTest() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    base::RunLoop loop;
    quit_closure_ = loop.QuitClosure();
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&PicasaDataProviderTest::SetupFoldersAndDataProvider,
                   base::Unretained(this)));
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&PicasaDataProviderTest::InitializeTestData,
                   base::Unretained(this)));
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&PicasaDataProviderTest::StartTestOnMediaTaskRunner,
                   base::Unretained(this)));
    loop.Run();
  }

  virtual PicasaDataProvider::DataType RequestedDataType() const = 0;

  // Start the test. The data provider is refreshed before calling StartTest
  // and the result of the refresh is passed in.
  virtual void VerifyRefreshResults(bool parse_success) {}

  void TestDone() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    // The data provider must be destructed on the MediaTaskRunner. This is done
    // in a posted task rather than directly because TestDone is called by
    // PicasaDataProvider. The callee should not destroy the caller.
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&PicasaDataProviderTest::DestructDataProviderThenQuit,
                   base::Unretained(this)));
  }

  const base::FilePath& test_folder_1_path() {
    return test_folder_1_.GetPath();
  }
  const base::FilePath& test_folder_2_path() {
    return test_folder_2_.GetPath();
  }

  TestPicasaDataProvider* data_provider() const {
    return picasa_data_provider_.get();
  }

  const base::FilePath GetTempDirPath() const {
    return picasa_root_dir_.GetPath().AppendASCII(kPicasaTempDirName);
  }

  virtual base::FilePath GetColumnFileDestination() const {
    return picasa_root_dir_.GetPath().AppendASCII(kPicasaDatabaseDirName);
  }

 private:
  void SetupFoldersAndDataProvider() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
    ASSERT_TRUE(test_folder_1_.CreateUniqueTempDir());
    ASSERT_TRUE(test_folder_2_.CreateUniqueTempDir());
    ASSERT_TRUE(picasa_root_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateDirectory(
        picasa_root_dir_.GetPath().AppendASCII(kPicasaDatabaseDirName)));
    ASSERT_TRUE(base::CreateDirectory(
        picasa_root_dir_.GetPath().AppendASCII(kPicasaTempDirName)));

    picasa_data_provider_.reset(new TestPicasaDataProvider(
        picasa_root_dir_.GetPath().AppendASCII(kPicasaDatabaseDirName)));
  }

  virtual void StartTestOnMediaTaskRunner() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    data_provider()->RefreshData(
        RequestedDataType(),
        base::Bind(&PicasaDataProviderTest::VerifyRefreshResults,
                   base::Unretained(this)));
  }

  void DestructDataProviderThenQuit() {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
    picasa_data_provider_.reset();
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, quit_closure_);
  }

  base::ScopedTempDir test_folder_1_;
  base::ScopedTempDir test_folder_2_;
  base::ScopedTempDir picasa_root_dir_;

  std::unique_ptr<TestPicasaDataProvider> picasa_data_provider_;

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(PicasaDataProviderTest);
};

class PicasaDataProviderNoDatabaseGetListTest : public PicasaDataProviderTest {
 protected:
  PicasaDataProvider::DataType RequestedDataType() const override {
    return PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA;
  }
  void VerifyRefreshResults(bool parse_success) override {
    EXPECT_FALSE(parse_success);
    TestDone();
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderNoDatabaseGetListTest,
                       NoDatabaseGetList) {
  RunTest();
}

class PicasaDataProviderNoDatabaseGetAlbumsImagesTest
    : public PicasaDataProviderTest {
 protected:
  PicasaDataProvider::DataType RequestedDataType() const override {
    return PicasaDataProvider::ALBUMS_IMAGES_DATA;
  }
  void VerifyRefreshResults(bool parse_success) override {
    EXPECT_FALSE(parse_success);
    TestDone();
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderNoDatabaseGetAlbumsImagesTest,
                       NoDatabaseGetAlbumsImages) {
  RunTest();
}

class PicasaDataProviderGetListTest : public PicasaDataProviderTest {
 protected:
  void InitializeTestData() override {
    WriteTestAlbumTable(GetColumnFileDestination(), test_folder_1_path(),
                        test_folder_2_path());
  }

  PicasaDataProvider::DataType RequestedDataType() const override {
    return PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA;
  }

  void VerifyRefreshResults(bool parse_success) override {
    ASSERT_TRUE(parse_success);
    VerifyTestAlbumTable(
        data_provider(), test_folder_1_path(), test_folder_2_path());
    TestDone();
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderGetListTest, GetListTest) {
  RunTest();
}

class PicasaDataProviderGetAlbumsImagesTest : public PicasaDataProviderTest {
 protected:
  void InitializeTestData() override {
    WriteTestAlbumTable(GetColumnFileDestination(), test_folder_1_path(),
                        test_folder_2_path());
    WriteTestAlbumsImagesIndex(test_folder_1_path(), test_folder_2_path());
  }

  PicasaDataProvider::DataType RequestedDataType() const override {
    return PicasaDataProvider::ALBUMS_IMAGES_DATA;
  }

  void VerifyRefreshResults(bool parse_success) override {
    ASSERT_TRUE(parse_success);
    VerifyTestAlbumTable(
        data_provider(), test_folder_1_path(), test_folder_2_path());
    VerifyTestAlbumsImagesIndex(
        data_provider(), test_folder_1_path(), test_folder_2_path());
    TestDone();
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderGetAlbumsImagesTest,
                       GetAlbumsImagesTest) {
  RunTest();
}

class PicasaDataProviderMultipleMixedCallbacksTest
    : public PicasaDataProviderTest {
 public:
  PicasaDataProviderMultipleMixedCallbacksTest()
      : list_callbacks_called_(0), albums_images_callbacks_called_(0) {}

  void InitializeTestData() override {
    WriteTestAlbumTable(GetColumnFileDestination(), test_folder_1_path(),
                        test_folder_2_path());
    WriteTestAlbumsImagesIndex(test_folder_1_path(), test_folder_2_path());
  }

  PicasaDataProvider::DataType RequestedDataType() const override {
    return PicasaDataProvider::ALBUMS_IMAGES_DATA;
  }

 protected:
  virtual void ListCallback(int expected_list_callbacks_called,
                            bool parse_success) {
    ASSERT_TRUE(parse_success);
    ASSERT_EQ(expected_list_callbacks_called, ++list_callbacks_called_);
    VerifyTestAlbumTable(
        data_provider(), test_folder_1_path(), test_folder_2_path());
    CheckTestDone();
  }

  virtual void AlbumsImagesCallback(int expected_albums_images_callbacks_called,
                                    bool parse_success) {
    ASSERT_TRUE(parse_success);
    ASSERT_EQ(expected_albums_images_callbacks_called,
              ++albums_images_callbacks_called_);
    VerifyTestAlbumsImagesIndex(
        data_provider(), test_folder_1_path(), test_folder_2_path());
    CheckTestDone();
  }

 private:
  void CheckTestDone() {
    ASSERT_LE(list_callbacks_called_, 2);
    ASSERT_LE(albums_images_callbacks_called_, 2);
    if (list_callbacks_called_ == 2 && albums_images_callbacks_called_ == 2)
      TestDone();
  }

  void StartTestOnMediaTaskRunner() override {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    data_provider()->RefreshData(
        PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA,
        base::Bind(&PicasaDataProviderMultipleMixedCallbacksTest::ListCallback,
                   base::Unretained(this),
                   1));
    data_provider()->RefreshData(
        PicasaDataProvider::ALBUMS_IMAGES_DATA,
        base::Bind(
            &PicasaDataProviderMultipleMixedCallbacksTest::AlbumsImagesCallback,
            base::Unretained(this),
            1));
    data_provider()->RefreshData(
        PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA,
        base::Bind(&PicasaDataProviderMultipleMixedCallbacksTest::ListCallback,
                   base::Unretained(this),
                   2));
    data_provider()->RefreshData(
        PicasaDataProvider::ALBUMS_IMAGES_DATA,
        base::Bind(
            &PicasaDataProviderMultipleMixedCallbacksTest::AlbumsImagesCallback,
            base::Unretained(this),
            2));
  }

  int list_callbacks_called_;
  int albums_images_callbacks_called_;
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderMultipleMixedCallbacksTest,
                       MultipleMixedCallbacks) {
  RunTest();
}

class PicasaDataProviderFileWatcherInvalidateTest
    : public PicasaDataProviderGetListTest {
 protected:
  virtual void ListCallback(bool parse_success) {
    ASSERT_FALSE(parse_success);

    // Validate the list after the file move triggers an invalidate.
    data_provider()->SetInvalidateCallback(base::Bind(
        &PicasaDataProvider::RefreshData,
        base::Unretained(data_provider()),
        RequestedDataType(),
        base::Bind(
            &PicasaDataProviderFileWatcherInvalidateTest::VerifyRefreshResults,
            base::Unretained(this))));

    data_provider()->MoveTempFilesToDatabase();
  }

  base::FilePath GetColumnFileDestination() const override {
    return GetTempDirPath();
  }

 private:
  void StartTestOnMediaTaskRunner() override {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    // Refresh before moving album table to database dir, guaranteeing failure.
    data_provider()->RefreshData(
        RequestedDataType(),
        base::Bind(
            &PicasaDataProviderFileWatcherInvalidateTest::ListCallback,
            base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderFileWatcherInvalidateTest,
                       FileWatcherInvalidateTest) {
  RunTest();
}

class PicasaDataProviderInvalidateInflightTableReaderTest
    : public PicasaDataProviderGetListTest {
 protected:
  // Don't write the database files until later.
  void InitializeTestData() override {}

 private:
  void StartTestOnMediaTaskRunner() override {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    // Refresh before the database files have been written.
    // This is guaranteed to fail to read the album table.
    data_provider()->RefreshData(
        RequestedDataType(),
        base::Bind(&PicasaDataProviderInvalidateInflightTableReaderTest::
                       VerifyRefreshResults,
                   base::Unretained(this)));

    // Now write the album table and invalidate the inflight table reader.
    PicasaDataProviderGetListTest::InitializeTestData();
    data_provider()->InvalidateData();

    // VerifyRefreshResults callback should receive correct results now.
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderInvalidateInflightTableReaderTest,
                       InvalidateInflightTableReaderTest) {
  RunTest();
}

class PicasaDataProviderInvalidateInflightAlbumsIndexerTest
    : public PicasaDataProviderGetAlbumsImagesTest {
 protected:
  virtual void ListCallback(bool parse_success) {
    ASSERT_TRUE(parse_success);

    // Empty the album maps to guarantee that the first utility process will
    // fail to get the correct albums-images index.
    data_provider()->SetAlbumMapsForTesting(AlbumMap(), AlbumMap());
    data_provider()->RefreshData(
        PicasaDataProvider::ALBUMS_IMAGES_DATA,
        base::Bind(&PicasaDataProviderInvalidateInflightAlbumsIndexerTest::
                       VerifyRefreshResults,
                   base::Unretained(this)));

    // Now invalidate all the data. The album maps will be re-read.
    data_provider()->InvalidateData();

    // VerifyRefreshResults callback should receive correct results now.
  }

 private:
  void StartTestOnMediaTaskRunner() override {
    MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();

    data_provider()->RefreshData(
        PicasaDataProvider::LIST_OF_ALBUMS_AND_FOLDERS_DATA,
        base::Bind(&PicasaDataProviderInvalidateInflightAlbumsIndexerTest::
                       ListCallback,
                   base::Unretained(this)));
  }
};

IN_PROC_BROWSER_TEST_F(PicasaDataProviderInvalidateInflightAlbumsIndexerTest,
                       InvalidateInflightAlbumsIndexerTest) {
  RunTest();
}

}  // namespace picasa
