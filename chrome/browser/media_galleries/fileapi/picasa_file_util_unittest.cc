// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/picasa_file_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/picasa_data_provider.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "chrome/common/media_galleries/picasa_types.h"
#include "chrome/common/media_galleries/pmp_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_file_system_options.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemOperationContext;
using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace picasa {

namespace {

base::Time::Exploded test_date_exploded = { 2013, 4, 0, 16, 0, 0, 0, 0 };

bool WriteJPEGHeader(const base::FilePath& path) {
  const char kJpegHeader[] = "\xFF\xD8\xFF";  // Per HTML5 specification.
  return base::WriteFile(path, kJpegHeader, arraysize(kJpegHeader)) != -1;
}

class TestFolder {
 public:
  TestFolder(const std::string& name, const base::Time& timestamp,
             const std::string& uid, unsigned int image_files,
             unsigned int non_image_files)
      : name_(name),
        timestamp_(timestamp),
        uid_(uid),
        image_files_(image_files),
        non_image_files_(non_image_files),
        folder_info_("", base::Time(), "", base::FilePath()) {
  }

  bool Init() {
    if (!folder_dir_.CreateUniqueTempDir())
      return false;

    folder_info_ = AlbumInfo(name_, timestamp_, uid_, folder_dir_.GetPath());

    for (unsigned int i = 0; i < image_files_; ++i) {
      std::string image_filename = base::StringPrintf("img%05d.jpg", i);
      image_filenames_.insert(image_filename);

      base::FilePath path = folder_dir_.GetPath().AppendASCII(image_filename);

      if (!WriteJPEGHeader(path))
        return false;
    }

    for (unsigned int i = 0; i < non_image_files_; ++i) {
      base::FilePath path = folder_dir_.GetPath().AppendASCII(
          base::StringPrintf("hello%05d.txt", i));
      if (base::WriteFile(path, NULL, 0) == -1)
        return false;
    }

    return true;
  }

  double GetVariantTimestamp() const {
    DCHECK(!folder_dir_.GetPath().empty());
    base::Time variant_epoch;
    EXPECT_TRUE(base::Time::FromLocalExploded(picasa::kPmpVariantTimeEpoch,
                                              &variant_epoch));

    int64_t microseconds_since_epoch =
        (folder_info_.timestamp - variant_epoch).InMicroseconds();

    return static_cast<double>(microseconds_since_epoch) /
                               base::Time::kMicrosecondsPerDay;
  }

  const std::set<std::string>& image_filenames() const {
    DCHECK(!folder_dir_.GetPath().empty());
    return image_filenames_;
  }

  const AlbumInfo& folder_info() const {
    DCHECK(!folder_dir_.GetPath().empty());
    return folder_info_;
  }

  const base::Time& timestamp() const {
    return timestamp_;
  }

 private:
  const std::string name_;
  const base::Time timestamp_;
  const std::string uid_;
  unsigned int image_files_;
  unsigned int non_image_files_;

  std::set<std::string> image_filenames_;

  base::ScopedTempDir folder_dir_;
  AlbumInfo folder_info_;
};

void ReadDirectoryTestHelperCallback(
    base::RunLoop* run_loop,
    FileSystemOperation::FileEntryList* contents,
    bool* completed, base::File::Error error,
    const FileSystemOperation::FileEntryList& file_list,
    bool has_more) {
  DCHECK(!*completed);
  *completed = !has_more && error == base::File::FILE_OK;
  *contents = file_list;
  run_loop->Quit();
}

void ReadDirectoryTestHelper(storage::FileSystemOperationRunner* runner,
                             const FileSystemURL& url,
                             FileSystemOperation::FileEntryList* contents,
                             bool* completed) {
  DCHECK(contents);
  DCHECK(completed);
  base::RunLoop run_loop;
  runner->ReadDirectory(
      url, base::Bind(&ReadDirectoryTestHelperCallback, &run_loop, contents,
                      completed));
  run_loop.Run();
}

void SynchronouslyRunOnMediaTaskRunner(const base::Closure& closure) {
  base::RunLoop loop;
  MediaFileSystemBackend::MediaTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      closure,
      loop.QuitClosure());
  loop.Run();
}

void CreateSnapshotFileTestHelperCallback(
    base::RunLoop* run_loop,
    base::File::Error* error,
    base::FilePath* platform_path_result,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<storage::ShareableFileReference>& file_ref) {
  DCHECK(run_loop);
  DCHECK(error);
  DCHECK(platform_path_result);

  *error = result;
  *platform_path_result = platform_path;
  run_loop->Quit();
}

}  // namespace

class TestPicasaFileUtil : public PicasaFileUtil {
 public:
  TestPicasaFileUtil(MediaPathFilter* media_path_filter,
                     PicasaDataProvider* data_provider)
      : PicasaFileUtil(media_path_filter),
        data_provider_(data_provider) {
  }
  ~TestPicasaFileUtil() override {}

 private:
  PicasaDataProvider* GetDataProvider() override { return data_provider_; }

  PicasaDataProvider* data_provider_;
};

class TestMediaFileSystemBackend : public MediaFileSystemBackend {
 public:
  TestMediaFileSystemBackend(const base::FilePath& profile_path,
                             PicasaFileUtil* picasa_file_util)
      : MediaFileSystemBackend(profile_path,
                               MediaFileSystemBackend::MediaTaskRunner().get()),
        test_file_util_(picasa_file_util) {}

  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override {
    if (type != storage::kFileSystemTypePicasa)
      return NULL;

    return test_file_util_.get();
  }

 private:
  std::unique_ptr<storage::AsyncFileUtil> test_file_util_;
};

class PicasaFileUtilTest : public testing::Test {
 public:
  PicasaFileUtilTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  void SetUp() override {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ImportedMediaGalleryRegistry::GetInstance()->Initialize();

    scoped_refptr<storage::SpecialStoragePolicy> storage_policy =
        new content::MockSpecialStoragePolicy();

    SynchronouslyRunOnMediaTaskRunner(base::Bind(
        &PicasaFileUtilTest::SetUpOnMediaTaskRunner, base::Unretained(this)));

    media_path_filter_.reset(new MediaPathFilter());

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(base::MakeUnique<TestMediaFileSystemBackend>(
        profile_dir_.GetPath(),
        new TestPicasaFileUtil(media_path_filter_.get(),
                               picasa_data_provider_.get())));

    file_system_context_ = new storage::FileSystemContext(
        base::ThreadTaskRunnerHandle::Get().get(),
        base::ThreadTaskRunnerHandle::Get().get(),
        storage::ExternalMountPoints::CreateRefCounted().get(),
        storage_policy.get(), NULL, std::move(additional_providers),
        std::vector<storage::URLRequestAutoMountHandler>(),
        profile_dir_.GetPath(), content::CreateAllowFileAccessOptions());
  }

  void TearDown() override {
    SynchronouslyRunOnMediaTaskRunner(
        base::Bind(&PicasaFileUtilTest::TearDownOnMediaTaskRunner,
                   base::Unretained(this)));
  }

 protected:
  void SetUpOnMediaTaskRunner() {
    picasa_data_provider_.reset(new PicasaDataProvider(base::FilePath()));
  }

  void TearDownOnMediaTaskRunner() {
    picasa_data_provider_.reset();
  }

  // |test_folders| must be in alphabetical order for easy verification
  void SetupFolders(std::vector<std::unique_ptr<TestFolder>>* test_folders,
                    const std::vector<AlbumInfo>& albums,
                    const AlbumImagesMap& albums_images) {
    std::vector<AlbumInfo> folders;
    for (const auto& test_folder : *test_folders) {
      ASSERT_TRUE(test_folder->Init());
      folders.push_back(test_folder->folder_info());
    }

    PicasaDataProvider::UniquifyNames(albums,
                                      &picasa_data_provider_->album_map_);
    PicasaDataProvider::UniquifyNames(folders,
                                      &picasa_data_provider_->folder_map_);
    picasa_data_provider_->albums_images_ = albums_images;
    picasa_data_provider_->state_ =
        PicasaDataProvider::ALBUMS_IMAGES_FRESH_STATE;
  }

  void VerifyFolderDirectoryList(
      const std::vector<std::unique_ptr<TestFolder>>& test_folders) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(kPicasaDirFolders);
    bool completed = false;
    ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

    ASSERT_TRUE(completed);
    ASSERT_EQ(test_folders.size(), contents.size());

    for (size_t i = 0; i < contents.size(); ++i) {
      EXPECT_TRUE(contents[i].is_directory);

      FileSystemOperation::FileEntryList folder_contents;
      FileSystemURL folder_url = CreateURL(
          std::string(kPicasaDirFolders) + "/" +
          base::FilePath(contents[i].name).AsUTF8Unsafe());
      bool folder_read_completed = false;
      ReadDirectoryTestHelper(operation_runner(), folder_url, &folder_contents,
                              &folder_read_completed);

      EXPECT_TRUE(folder_read_completed);

      const std::set<std::string>& image_filenames =
          test_folders[i]->image_filenames();

      EXPECT_EQ(image_filenames.size(), folder_contents.size());

      for (auto file_it = folder_contents.begin();
           file_it != folder_contents.end(); ++file_it) {
        EXPECT_EQ(1u, image_filenames.count(
            base::FilePath(file_it->name).AsUTF8Unsafe()));
      }
    }
  }

  std::string DateToPathString(const base::Time& time) {
    return PicasaDataProvider::DateToPathString(time);
  }

  void TestNonexistentDirectory(const std::string& path) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(path);
    bool completed = false;
    ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

    ASSERT_FALSE(completed);
  }

  void TestEmptyDirectory(const std::string& path) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(path);
    bool completed = false;
    ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

    ASSERT_TRUE(completed);
    EXPECT_EQ(0u, contents.size());
  }

  FileSystemURL CreateURL(const std::string& path) const {
    base::FilePath virtual_path =
        ImportedMediaGalleryRegistry::GetInstance()->ImportedRoot();
    virtual_path = virtual_path.AppendASCII("picasa");
    virtual_path = virtual_path.AppendASCII(path);
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://www.example.com"),
        storage::kFileSystemTypePicasa,
        virtual_path);
  }

  storage::FileSystemOperationRunner* operation_runner() const {
    return file_system_context_->operation_runner();
  }

  scoped_refptr<storage::FileSystemContext> file_system_context() const {
    return file_system_context_;
  }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;

  base::ScopedTempDir profile_dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<PicasaDataProvider> picasa_data_provider_;
  std::unique_ptr<MediaPathFilter> media_path_filter_;

  DISALLOW_COPY_AND_ASSIGN(PicasaFileUtilTest);
};

TEST_F(PicasaFileUtilTest, DateFormat) {
  base::Time::Exploded exploded_shortmonth = { 2013, 4, 0, 16, 0, 0, 0, 0 };
  base::Time shortmonth;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_shortmonth, &shortmonth));

  base::Time::Exploded exploded_shortday = { 2013, 11, 0, 3, 0, 0, 0, 0 };
  base::Time shortday;
  EXPECT_TRUE(base::Time::FromLocalExploded(exploded_shortday, &shortday));

  EXPECT_EQ("2013-04-16", DateToPathString(shortmonth));
  EXPECT_EQ("2013-11-03", DateToPathString(shortday));
}

TEST_F(PicasaFileUtilTest, NameDeduplication) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  std::vector<std::string> expected_names;

  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));
  base::Time test_date_2 = test_date - base::TimeDelta::FromDays(1);

  std::string test_date_string = DateToPathString(test_date);
  std::string test_date_2_string = DateToPathString(test_date_2);

  test_folders.push_back(
      base::MakeUnique<TestFolder>("diff_date", test_date_2, "uuid3", 0, 0));
  expected_names.push_back("diff_date " + test_date_2_string);

  test_folders.push_back(
      base::MakeUnique<TestFolder>("diff_date", test_date, "uuid2", 0, 0));
  expected_names.push_back("diff_date " + test_date_string);

  test_folders.push_back(
      base::MakeUnique<TestFolder>("duplicate", test_date, "uuid4", 0, 0));
  expected_names.push_back("duplicate " + test_date_string + " (1)");

  test_folders.push_back(
      base::MakeUnique<TestFolder>("duplicate", test_date, "uuid5", 0, 0));
  expected_names.push_back("duplicate " + test_date_string + " (2)");

  test_folders.push_back(
      base::MakeUnique<TestFolder>("unique_name", test_date, "uuid1", 0, 0));
  expected_names.push_back("unique_name " + test_date_string);

  SetupFolders(&test_folders, std::vector<AlbumInfo>(), AlbumImagesMap());

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(kPicasaDirFolders);
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(expected_names.size(), contents.size());
  for (size_t i = 0; i < contents.size(); ++i) {
    EXPECT_EQ(expected_names[i],
              base::FilePath(contents[i].name).AsUTF8Unsafe());
    EXPECT_TRUE(contents[i].is_directory);
  }
}

TEST_F(PicasaFileUtilTest, RootFolders) {
  std::vector<std::unique_ptr<TestFolder>> empty_folders_list;
  SetupFolders(&empty_folders_list, std::vector<AlbumInfo>(), AlbumImagesMap());

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL("");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_TRUE(contents.back().is_directory);

  EXPECT_EQ(FILE_PATH_LITERAL("albums"), contents.front().name);
  EXPECT_EQ(FILE_PATH_LITERAL("folders"), contents.back().name);
}

TEST_F(PicasaFileUtilTest, NonexistentFolder) {
  std::vector<std::unique_ptr<TestFolder>> empty_folders_list;
  SetupFolders(&empty_folders_list, std::vector<AlbumInfo>(), AlbumImagesMap());

  TestNonexistentDirectory(std::string(kPicasaDirFolders) + "/foo");
  TestNonexistentDirectory(std::string(kPicasaDirFolders) + "/foo/bar");
  TestNonexistentDirectory(std::string(kPicasaDirFolders) + "/foo/bar/baz");
}

TEST_F(PicasaFileUtilTest, FolderContentsTrivial) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));

  test_folders.push_back(base::MakeUnique<TestFolder>(
      "folder-1-empty", test_date, "uid-empty", 0, 0));
  test_folders.push_back(base::MakeUnique<TestFolder>(
      "folder-2-images", test_date, "uid-images", 5, 0));
  test_folders.push_back(base::MakeUnique<TestFolder>(
      "folder-3-nonimages", test_date, "uid-nonimages", 0, 5));
  test_folders.push_back(base::MakeUnique<TestFolder>(
      "folder-4-both", test_date, "uid-both", 5, 5));

  SetupFolders(&test_folders, std::vector<AlbumInfo>(), AlbumImagesMap());
  VerifyFolderDirectoryList(test_folders);
}

TEST_F(PicasaFileUtilTest, FolderWithManyFiles) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));

  test_folders.push_back(base::MakeUnique<TestFolder>(
      "folder-many-files", test_date, "uid-both", 50, 50));

  SetupFolders(&test_folders, std::vector<AlbumInfo>(), AlbumImagesMap());
  VerifyFolderDirectoryList(test_folders);
}

TEST_F(PicasaFileUtilTest, ManyFolders) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));

  for (unsigned int i = 0; i < 50; ++i) {
    base::Time date = test_date - base::TimeDelta::FromDays(i);

    test_folders.push_back(base::MakeUnique<TestFolder>(
        base::StringPrintf("folder-%05d", i), date,
        base::StringPrintf("uid%05d", i), i % 5, i % 3));
  }

  SetupFolders(&test_folders, std::vector<AlbumInfo>(), AlbumImagesMap());
  VerifyFolderDirectoryList(test_folders);
}

TEST_F(PicasaFileUtilTest, AlbumExistence) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));

  std::vector<AlbumInfo> albums;
  AlbumInfo info;
  info.name = "albumname";
  info.uid = "albumuid";
  info.timestamp = test_date;
  albums.push_back(info);

  AlbumImagesMap albums_images;
  albums_images[info.uid] = AlbumImages();

  SetupFolders(&test_folders, albums, albums_images);

  TestEmptyDirectory(std::string(kPicasaDirAlbums) + "/albumname 2013-04-16");
  TestNonexistentDirectory(std::string(kPicasaDirAlbums) +
                           "/albumname 2013-04-16/toodeep");
  TestNonexistentDirectory(std::string(kPicasaDirAlbums) + "/wrongname");
}

TEST_F(PicasaFileUtilTest, AlbumContents) {
  std::vector<std::unique_ptr<TestFolder>> test_folders;
  base::Time test_date;
  EXPECT_TRUE(base::Time::FromLocalExploded(test_date_exploded, &test_date));

  std::vector<AlbumInfo> albums;
  AlbumInfo info;
  info.name = "albumname";
  info.uid = "albumuid";
  info.timestamp = test_date;
  albums.push_back(info);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath image_path = temp_dir.GetPath().AppendASCII("img.jpg");
  ASSERT_TRUE(WriteJPEGHeader(image_path));

  AlbumImagesMap albums_images;
  albums_images[info.uid] = AlbumImages();
  albums_images[info.uid]["mapped_name.jpg"] = image_path;

  SetupFolders(&test_folders, albums, albums_images);

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url =
      CreateURL(std::string(kPicasaDirAlbums) + "/albumname 2013-04-16");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  EXPECT_EQ(1u, contents.size());
  EXPECT_EQ("mapped_name.jpg",
            base::FilePath(contents.begin()->name).AsUTF8Unsafe());
  EXPECT_FALSE(contents.begin()->is_directory);

  // Create a snapshot file to verify the file path.
  base::RunLoop loop;
  base::File::Error error;
  base::FilePath platform_path_result;
  storage::FileSystemOperationRunner::SnapshotFileCallback snapshot_callback =
      base::Bind(&CreateSnapshotFileTestHelperCallback,
                 &loop,
                 &error,
                 &platform_path_result);
  operation_runner()->CreateSnapshotFile(
      CreateURL(std::string(kPicasaDirAlbums) +
                "/albumname 2013-04-16/mapped_name.jpg"),
      snapshot_callback);
  loop.Run();
  EXPECT_EQ(base::File::FILE_OK, error);
  EXPECT_EQ(image_path, platform_path_result);
}

}  // namespace picasa
