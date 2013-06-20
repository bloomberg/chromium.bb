// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_album_table_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_file_util.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_test_helper.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/mock_file_system_options.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"

using fileapi::FileSystemFileUtil;
using fileapi::FileSystemOperationContext;
using fileapi::FileSystemOperation;
using fileapi::FileSystemURL;

namespace picasa {

namespace {

base::Time::Exploded test_date_exploded = { 2013, 4, 0, 16, 0, 0, 0, 0 };

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

    folder_info_ = AlbumInfo(name_, timestamp_, uid_, folder_dir_.path());

    const char kJpegHeader[] = "\xFF\xD8\xFF";  // Per HTML5 specification.
    for (unsigned int i = 0; i < image_files_; ++i) {
      std::string image_filename = base::StringPrintf("img%05d.jpg", i);
      image_filenames_.insert(image_filename);

      base::FilePath path = folder_dir_.path().AppendASCII(image_filename);

      if (file_util::WriteFile(path, kJpegHeader, arraysize(kJpegHeader)) == -1)
        return false;
    }

    for (unsigned int i = 0; i < non_image_files_; ++i) {
      base::FilePath path = folder_dir_.path().AppendASCII(
          base::StringPrintf("hello%05d.txt", i));
      if (file_util::WriteFile(path, NULL, 0) == -1)
        return false;
    }

    return true;
  }

  double GetVariantTimestamp() const {
    DCHECK(!folder_dir_.path().empty());
    base::Time variant_epoch = base::Time::FromLocalExploded(
        picasa::kPicasaVariantTimeEpoch);

    int64 microseconds_since_epoch =
        (folder_info_.timestamp - variant_epoch).InMicroseconds();

    return static_cast<double>(microseconds_since_epoch) /
                               base::Time::kMicrosecondsPerDay;
  }

  const std::set<std::string>& image_filenames() const {
    DCHECK(!folder_dir_.path().empty());
    return image_filenames_;
  }

  const AlbumInfo& folder_info() const {
    DCHECK(!folder_dir_.path().empty());
    return folder_info_;
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

class TestPicasaDataProvider : public PicasaDataProvider {
 public:
  TestPicasaDataProvider(const std::vector<AlbumInfo>& albums,
                         const std::vector<AlbumInfo>& folders)
      : PicasaDataProvider(base::FilePath(FILE_PATH_LITERAL("Fake"))) {
    InitializeWith(albums, folders);
  }

 private:
  virtual bool ReadData() OVERRIDE {
    return true;
  }
  virtual ~TestPicasaDataProvider() {}
};

class TestPicasaFileUtil : public PicasaFileUtil {
 public:
  explicit TestPicasaFileUtil(PicasaDataProvider* data_provider)
      : data_provider_(data_provider) {
  }
  virtual ~TestPicasaFileUtil() {}
 private:
  virtual PicasaDataProvider* GetDataProvider() OVERRIDE {
    return data_provider_.get();
  }

  scoped_ptr<PicasaDataProvider> data_provider_;
};

class TestMediaFileSystemMountPointProvider
    : public chrome::MediaFileSystemMountPointProvider {
 public:
  TestMediaFileSystemMountPointProvider(const base::FilePath& profile_path,
                                        PicasaFileUtil* picasa_file_util)
      : chrome::MediaFileSystemMountPointProvider(
            profile_path,
            base::MessageLoopProxy::current().get()),
        test_file_util_(picasa_file_util) {}

  virtual fileapi::AsyncFileUtil*
  GetAsyncFileUtil(fileapi::FileSystemType type) OVERRIDE {
    if (type != fileapi::kFileSystemTypePicasa)
      return NULL;

    return test_file_util_.get();
  }

 private:
  scoped_ptr<fileapi::AsyncFileUtil> test_file_util_;
};

void DidReadDirectory(FileSystemOperation::FileEntryList* contents,
                      bool* completed, base::PlatformFileError error,
                      const FileSystemOperation::FileEntryList& file_list,
                      bool has_more) {
  DCHECK(!*completed);
  *completed = !has_more && error == base::PLATFORM_FILE_OK;
  *contents = file_list;
}

void ReadDirectoryTestCallback(
    base::RunLoop* run_loop,
    base::PlatformFileError* error_result,
    fileapi::AsyncFileUtil::EntryList* file_list_result,
    base::PlatformFileError error,
    const fileapi::AsyncFileUtil::EntryList& file_list,
    bool /*has_more*/) {
  DCHECK(error_result);
  DCHECK(file_list_result);
  *error_result = error;
  *file_list_result = file_list;
  run_loop->Quit();
}

base::PlatformFileError ReadDirectoryTestHelper(
    fileapi::AsyncFileUtil* file_util,
    scoped_ptr<FileSystemOperationContext> operation_context,
    FileSystemURL url,
    fileapi::AsyncFileUtil::EntryList* file_list) {
  base::RunLoop run_loop;
  base::PlatformFileError result;
  file_util->ReadDirectory(
      operation_context.Pass(),
      url,
      base::Bind(&ReadDirectoryTestCallback, &run_loop, &result, file_list));
  run_loop.Run();
  return result;
}

}  // namespace

class PicasaFileUtilTest : public testing::Test {
 public:
  PicasaFileUtilTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }
  virtual ~PicasaFileUtilTest() {}

  virtual void SetUp() OVERRIDE {
    test_helper_.reset(new PmpTestHelper(kPicasaAlbumTableName));
    ASSERT_TRUE(test_helper_->Init());

    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());

    scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
        new quota::MockSpecialStoragePolicy();

    ScopedVector<fileapi::FileSystemMountPointProvider> additional_providers;
    additional_providers.push_back(new TestMediaFileSystemMountPointProvider(
        profile_dir_.path(),
        new TestPicasaFileUtil(
            new PicasaDataProvider(test_helper_->GetTempDirPath()))));

    file_system_context_ = new fileapi::FileSystemContext(
        fileapi::FileSystemTaskRunners::CreateMockTaskRunners(),
        fileapi::ExternalMountPoints::CreateRefCounted().get(),
        storage_policy.get(),
        NULL,
        additional_providers.Pass(),
        profile_dir_.path(),
        fileapi::CreateAllowFileAccessOptions());
  }

 protected:
  // |test_folders| must be in alphabetical order for easy verification
  void SetupFolders(ScopedVector<TestFolder>* test_folders) {
    // Build up pmp files.
    std::vector<uint32> category_column;
    std::vector<double> date_column;
    std::vector<std::string> filename_column;
    std::vector<std::string> name_column;
    std::vector<std::string> uid_column;
    std::vector<std::string> token_column;

    for (ScopedVector<TestFolder>::iterator it = test_folders->begin();
        it != test_folders->end(); ++it) {
      TestFolder* test_folder = *it;
      ASSERT_TRUE(test_folder->Init());
      category_column.push_back(picasa::kAlbumCategoryFolder);
      date_column.push_back(test_folder->GetVariantTimestamp());
      filename_column.push_back(test_folder->folder_info().path.AsUTF8Unsafe());
      name_column.push_back(test_folder->folder_info().name);
      uid_column.push_back(test_folder->folder_info().uid);
      token_column.push_back("]album:" + test_folder->folder_info().uid);
    }

    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "category", picasa::PMP_TYPE_UINT32, category_column));
    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "date", picasa::PMP_TYPE_DOUBLE64, date_column));
    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "filename", picasa::PMP_TYPE_STRING, filename_column));
    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "name", picasa::PMP_TYPE_STRING, name_column));
    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "uid", picasa::PMP_TYPE_STRING, uid_column));
    ASSERT_TRUE(test_helper_->WriteColumnFileFromVector(
        "token", picasa::PMP_TYPE_STRING, token_column));
  }

  void VerifyFolderDirectoryList(const ScopedVector<TestFolder>& test_folders) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(kPicasaDirFolders);
    bool completed = false;
    operation_runner()->ReadDirectory(
        url, base::Bind(&DidReadDirectory, &contents, &completed));
    base::MessageLoop::current()->RunUntilIdle();

    ASSERT_TRUE(completed);
    ASSERT_EQ(test_folders.size(), contents.size());

    for (size_t i = 0; i < contents.size(); ++i) {
      EXPECT_TRUE(contents[i].is_directory);

      // Because the timestamp is written out as a floating point Microsoft
      // variant time, we only expect it to be accurate to within a second.
      base::TimeDelta delta = test_folders[i]->folder_info().timestamp -
                              contents[i].last_modified_time;
      EXPECT_LT(delta, base::TimeDelta::FromSeconds(1));

      FileSystemOperation::FileEntryList folder_contents;
      FileSystemURL folder_url = CreateURL(
          std::string(kPicasaDirFolders) + "/" +
          base::FilePath(contents[i].name).AsUTF8Unsafe());
      bool folder_read_completed = false;
      operation_runner()->ReadDirectory(
          folder_url,
          base::Bind(&DidReadDirectory, &folder_contents,
                     &folder_read_completed));
      base::MessageLoop::current()->RunUntilIdle();

      EXPECT_TRUE(folder_read_completed);

      const std::set<std::string>& image_filenames =
          test_folders[i]->image_filenames();

      EXPECT_EQ(image_filenames.size(), folder_contents.size());

      for (FileSystemOperation::FileEntryList::const_iterator file_it =
               folder_contents.begin(); file_it != folder_contents.end();
           ++file_it) {
        EXPECT_EQ(1u, image_filenames.count(
            base::FilePath(file_it->name).AsUTF8Unsafe()));
      }
    }
  }

  std::string DateToPathString(const base::Time& time) {
    return PicasaDataProvider::DateToPathString(time);
  }

  void TestNonexistentFolder(const std::string& path_append) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(
        std::string(kPicasaDirFolders) + path_append);
    bool completed = false;
    operation_runner()->ReadDirectory(
        url, base::Bind(&DidReadDirectory, &contents, &completed));
    base::MessageLoop::current()->RunUntilIdle();

    ASSERT_FALSE(completed);
  }

  FileSystemURL CreateURL(const std::string& virtual_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://www.example.com"), fileapi::kFileSystemTypePicasa,
        base::FilePath::FromUTF8Unsafe(virtual_path));
  }

  fileapi::FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

  scoped_refptr<fileapi::FileSystemContext> file_system_context() {
    return file_system_context_;
  }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;

  base::ScopedTempDir profile_dir_;

  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_ptr<PmpTestHelper> test_helper_;

  DISALLOW_COPY_AND_ASSIGN(PicasaFileUtilTest);
};

TEST_F(PicasaFileUtilTest, DateFormat) {
  base::Time::Exploded exploded_shortmonth = { 2013, 4, 0, 16, 0, 0, 0, 0 };
  base::Time shortmonth = base::Time::FromLocalExploded(exploded_shortmonth);

  base::Time::Exploded exploded_shortday = { 2013, 11, 0, 3, 0, 0, 0, 0 };
  base::Time shortday = base::Time::FromLocalExploded(exploded_shortday);

  EXPECT_EQ("2013-04-16", DateToPathString(shortmonth));
  EXPECT_EQ("2013-11-03", DateToPathString(shortday));
}

TEST_F(PicasaFileUtilTest, NameDeduplication) {
  std::vector<AlbumInfo> folders;
  std::vector<std::string> expected_names;

  base::Time test_date = base::Time::FromLocalExploded(test_date_exploded);
  base::Time test_date_2 = test_date - base::TimeDelta::FromDays(1);

  std::string test_date_string = DateToPathString(test_date);
  std::string test_date_2_string = DateToPathString(test_date_2);

  folders.push_back(
      AlbumInfo("diff_date", test_date_2, "uuid3", base::FilePath()));
  expected_names.push_back("diff_date " + test_date_2_string);

  folders.push_back(
      AlbumInfo("diff_date", test_date, "uuid2", base::FilePath()));
  expected_names.push_back("diff_date " + test_date_string);

  folders.push_back(
      AlbumInfo("duplicate", test_date, "uuid4", base::FilePath()));
  expected_names.push_back("duplicate " + test_date_string + " (1)");

  folders.push_back(
      AlbumInfo("duplicate", test_date, "uuid5", base::FilePath()));
  expected_names.push_back("duplicate " + test_date_string + " (2)");

  folders.push_back(
      AlbumInfo("unique_name", test_date, "uuid1", base::FilePath()));
  expected_names.push_back("unique_name " + test_date_string);

  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(file_system_context().get()));

  scoped_ptr<chrome::MediaPathFilter> media_path_filter(
      new chrome::MediaPathFilter());

  operation_context->SetUserValue(
      chrome::MediaFileSystemMountPointProvider::kMediaPathFilterKey,
      media_path_filter.get());

  TestPicasaFileUtil test_file_util(
      new TestPicasaDataProvider(std::vector<AlbumInfo>(), folders));

  fileapi::AsyncFileUtil::EntryList file_list;
  ASSERT_EQ(base::PLATFORM_FILE_OK,
            ReadDirectoryTestHelper(&test_file_util, operation_context.Pass(),
                                    CreateURL(kPicasaDirFolders),
                                    &file_list));

  ASSERT_EQ(expected_names.size(), file_list.size());
  for (size_t i = 0; i < file_list.size(); ++i) {
    EXPECT_EQ(expected_names[i],
              base::FilePath(file_list[i].name).AsUTF8Unsafe());
    EXPECT_EQ(folders[i].timestamp, file_list[i].last_modified_time);
    EXPECT_TRUE(file_list[i].is_directory);
  }
}

TEST_F(PicasaFileUtilTest, RootFolders) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL("");
  bool completed = false;
  operation_runner()->ReadDirectory(
      url, base::Bind(&DidReadDirectory, &contents, &completed));
  base::MessageLoop::current()->RunUntilIdle();

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_TRUE(contents.back().is_directory);

  EXPECT_EQ(0, contents.front().size);
  EXPECT_EQ(0, contents.back().size);

  EXPECT_EQ(FILE_PATH_LITERAL("albums"), contents.front().name);
  EXPECT_EQ(FILE_PATH_LITERAL("folders"), contents.back().name);
}

TEST_F(PicasaFileUtilTest, NonexistentFolder) {
  ScopedVector<TestFolder> empty_folders_list;
  SetupFolders(&empty_folders_list);

  TestNonexistentFolder("/foo");
  TestNonexistentFolder("/foo/bar");
  TestNonexistentFolder("/foo/bar/baz");
}

TEST_F(PicasaFileUtilTest, FolderContentsTrivial) {
  ScopedVector<TestFolder> test_folders;
  base::Time test_date = base::Time::FromLocalExploded(test_date_exploded);

  test_folders.push_back(
      new TestFolder("folder-1-empty", test_date, "uid-empty", 0, 0));
  test_folders.push_back(
      new TestFolder("folder-2-images", test_date, "uid-images", 5, 0));
  test_folders.push_back(
      new TestFolder("folder-3-nonimages", test_date, "uid-nonimages", 0, 5));
  test_folders.push_back(
      new TestFolder("folder-4-both", test_date, "uid-both", 5, 5));

  SetupFolders(&test_folders);
  VerifyFolderDirectoryList(test_folders);
}

TEST_F(PicasaFileUtilTest, FolderWithManyFiles) {
  ScopedVector<TestFolder> test_folders;
  base::Time test_date = base::Time::FromLocalExploded(test_date_exploded);

  test_folders.push_back(
      new TestFolder("folder-many-files", test_date, "uid-both", 500, 500));

  SetupFolders(&test_folders);
  VerifyFolderDirectoryList(test_folders);
}

TEST_F(PicasaFileUtilTest, ManyFolders) {
  ScopedVector<TestFolder> test_folders;
  base::Time test_date = base::Time::FromLocalExploded(test_date_exploded);

  // TODO(tommycli): Turn number of test folders back up to 50 (or more) once
  // https://codereview.chromium.org/15479003/ lands.
  for (unsigned int i = 0; i < 25; ++i) {
    base::Time date = test_date - base::TimeDelta::FromDays(i);

    test_folders.push_back(
        new TestFolder(base::StringPrintf("folder-%05d", i),
                       date,
                       base::StringPrintf("uid%05d", i), i % 5, i % 3));
  }

  SetupFolders(&test_folders);
  VerifyFolderDirectoryList(test_folders);
}

}  // namespace picasa
