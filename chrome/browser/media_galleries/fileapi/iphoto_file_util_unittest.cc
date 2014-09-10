// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_file_system_options.h"
#include "storage/browser/fileapi/async_file_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using storage::FileSystemOperationContext;
using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace iphoto {

namespace {

void ReadDirectoryTestHelperCallback(
    base::RunLoop* run_loop,
    FileSystemOperation::FileEntryList* contents,
    bool* completed,
    base::File::Error error,
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

}  // namespace

class TestIPhotoDataProvider : public IPhotoDataProvider {
 public:
  explicit TestIPhotoDataProvider(const base::FilePath& fake_library_path)
     : IPhotoDataProvider(fake_library_path) {
    EXPECT_TRUE(fake_auto_add_dir_.CreateUniqueTempDir());
  }

  virtual ~TestIPhotoDataProvider() {}

  virtual void RefreshData(const ReadyCallback& ready_callback) OVERRIDE {
    ready_callback.Run(true /* success */);
  }

  virtual std::vector<std::string> GetAlbumNames() const OVERRIDE {
    std::vector<std::string> names;
    names.push_back("Album1");
    names.push_back("has_originals");
    return names;
  }

  virtual std::map<std::string, base::FilePath> GetAlbumContents(
      const std::string& album) const OVERRIDE {
    std::map<std::string, base::FilePath> contents;
    contents["a.jpg"] = library_path().AppendASCII("a.jpg");
    return contents;
  }

  virtual base::FilePath GetPhotoLocationInAlbum(
      const std::string& album,
      const std::string& filename) const OVERRIDE {
    return library_path().AppendASCII("a.jpg");
  }

  virtual bool HasOriginals(const std::string& album) const OVERRIDE {
    return (album == "has_originals");
  }

  virtual std::map<std::string, base::FilePath> GetOriginals(
      const std::string& album) const OVERRIDE {
    std::map<std::string, base::FilePath> contents;
    contents["a.jpg"] = library_path().AppendASCII("orig.jpg");
    return contents;
  }

  virtual base::FilePath GetOriginalPhotoLocation(
      const std::string& album,
      const std::string& filename) const OVERRIDE {
    return library_path().AppendASCII("orig.jpg");
  }

 private:
  base::ScopedTempDir fake_auto_add_dir_;
};

class TestIPhotoFileUtil : public IPhotoFileUtil {
 public:
  explicit TestIPhotoFileUtil(MediaPathFilter* media_path_filter,
                              IPhotoDataProvider* data_provider)
      : IPhotoFileUtil(media_path_filter),
        data_provider_(data_provider) {
  }
  virtual ~TestIPhotoFileUtil() {}

 private:
  virtual IPhotoDataProvider* GetDataProvider() OVERRIDE {
    return data_provider_;
  }

  IPhotoDataProvider* data_provider_;
};

class TestMediaFileSystemBackend : public MediaFileSystemBackend {
 public:
  TestMediaFileSystemBackend(const base::FilePath& profile_path,
                             IPhotoFileUtil* iphoto_file_util)
      : MediaFileSystemBackend(
            profile_path,
            MediaFileSystemBackend::MediaTaskRunner().get()),
        test_file_util_(iphoto_file_util) {}

  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) OVERRIDE {
    if (type != storage::kFileSystemTypeIphoto)
      return NULL;

    return test_file_util_.get();
  }

 private:
  scoped_ptr<storage::AsyncFileUtil> test_file_util_;
};

class IPhotoFileUtilTest : public testing::Test {
 public:
  IPhotoFileUtilTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }
  virtual ~IPhotoFileUtilTest() {}

  void SetUpDataProvider() {
    ASSERT_TRUE(fake_library_dir_.CreateUniqueTempDir());
    ASSERT_EQ(
        0,
        base::WriteFile(
            fake_library_dir_.path().AppendASCII("a.jpg"),
            NULL,
            0));
    ASSERT_EQ(
        0,
        base::WriteFile(
            fake_library_dir_.path().AppendASCII("orig.jpg"),
            NULL,
            0));

    iphoto_data_provider_.reset(
        new TestIPhotoDataProvider(fake_library_dir_.path()));
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ImportedMediaGalleryRegistry::GetInstance()->Initialize();

    scoped_refptr<storage::SpecialStoragePolicy> storage_policy =
        new content::MockSpecialStoragePolicy();

    // Initialize fake IPhotoDataProvider on media task runner thread.
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&IPhotoFileUtilTest::SetUpDataProvider,
                   base::Unretained(this)));
    base::WaitableEvent event(true, false /* initially_signalled */);
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&event)));
    event.Wait();

    media_path_filter_.reset(new MediaPathFilter());
    ScopedVector<storage::FileSystemBackend> additional_providers;
    additional_providers.push_back(new TestMediaFileSystemBackend(
        profile_dir_.path(),
        new TestIPhotoFileUtil(media_path_filter_.get(),
                               iphoto_data_provider_.get())));

    file_system_context_ = new storage::FileSystemContext(
        base::MessageLoopProxy::current().get(),
        base::MessageLoopProxy::current().get(),
        storage::ExternalMountPoints::CreateRefCounted().get(),
        storage_policy.get(),
        NULL,
        additional_providers.Pass(),
        std::vector<storage::URLRequestAutoMountHandler>(),
        profile_dir_.path(),
        content::CreateAllowFileAccessOptions());
  }

 protected:
  void TestNonexistentFolder(const std::string& path_append) {
    FileSystemOperation::FileEntryList contents;
    FileSystemURL url = CreateURL(path_append);
    bool completed = false;
    ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

    ASSERT_FALSE(completed);
  }

  FileSystemURL CreateURL(const std::string& path) const {
    base::FilePath virtual_path =
        ImportedMediaGalleryRegistry::GetInstance()->ImportedRoot();
    virtual_path = virtual_path.AppendASCII("iphoto");
    virtual_path = virtual_path.AppendASCII(path);
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://www.example.com"),
        storage::kFileSystemTypeIphoto,
        virtual_path);
  }

  storage::FileSystemOperationRunner* operation_runner() const {
    return file_system_context_->operation_runner();
  }

  scoped_refptr<storage::FileSystemContext> file_system_context() const {
    return file_system_context_;
  }

  TestIPhotoDataProvider* data_provider() const {
    return iphoto_data_provider_.get();
  }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;

  base::ScopedTempDir profile_dir_;
  base::ScopedTempDir fake_library_dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_ptr<MediaPathFilter> media_path_filter_;
  scoped_ptr<TestIPhotoDataProvider> iphoto_data_provider_;

  DISALLOW_COPY_AND_ASSIGN(IPhotoFileUtilTest);
};

TEST_F(IPhotoFileUtilTest, RootContents) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL("");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kIPhotoAlbumsDir).value(),
            contents.back().name);
}

TEST_F(IPhotoFileUtilTest, AlbumsDirectoryContents) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(kIPhotoAlbumsDir);
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);

  EXPECT_EQ("Album1", contents.front().name);
  EXPECT_EQ("has_originals", contents.back().name);
}

TEST_F(IPhotoFileUtilTest, AlbumContents) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(std::string(kIPhotoAlbumsDir) + "/Album1");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());

  EXPECT_FALSE(contents.front().is_directory);

  EXPECT_EQ("a.jpg", contents.back().name);
}

TEST_F(IPhotoFileUtilTest, BadAccess) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL("None");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_FALSE(completed);
  ASSERT_EQ(0u, contents.size());

  url = CreateURL(std::string(kIPhotoAlbumsDir) + "/NoAlbum");
  completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_FALSE(completed);
  ASSERT_EQ(0u, contents.size());
}

TEST_F(IPhotoFileUtilTest, Originals) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url =
      CreateURL(std::string(kIPhotoAlbumsDir) + "/has_originals");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());
  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_EQ("Originals", contents.front().name);
  EXPECT_FALSE(contents.back().is_directory);
  EXPECT_EQ("a.jpg", contents.back().name);

  url = CreateURL(std::string(kIPhotoAlbumsDir) + "/has_originals/Originals");
  completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());

  EXPECT_FALSE(contents.front().is_directory);
  EXPECT_EQ("a.jpg", contents.front().name);
}

}  // namespace iphoto
