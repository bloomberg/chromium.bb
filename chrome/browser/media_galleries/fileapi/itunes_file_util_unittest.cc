// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_special_storage_policy.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"

using storage::FileSystemOperationContext;
using storage::FileSystemOperation;
using storage::FileSystemURL;

namespace itunes {

namespace {

void ReadDirectoryTestHelperCallback(
    base::RunLoop* run_loop,
    FileSystemOperation::FileEntryList* contents,
    bool* completed, base::File::Error error,
    const FileSystemOperation::FileEntryList& file_list,
    bool has_more) {
  DCHECK(!*completed);
  *completed = (!has_more && error == base::File::FILE_OK);
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

class TestITunesDataProvider : public ITunesDataProvider {
 public:
  explicit TestITunesDataProvider(const base::FilePath& fake_library_path)
     : ITunesDataProvider(fake_library_path) {
    EXPECT_TRUE(fake_auto_add_dir_.CreateUniqueTempDir());
  }

  virtual ~TestITunesDataProvider() {}

  virtual void RefreshData(const ReadyCallback& ready_callback) OVERRIDE {
    ready_callback.Run(true /* success */);
  }

  virtual const base::FilePath& auto_add_path() const OVERRIDE {
    return fake_auto_add_dir_.path();
  }

  void SetProvideAutoAddDir(bool provide_auto_add_dir) {
    if (provide_auto_add_dir) {
      if (!fake_auto_add_dir_.IsValid())
        ASSERT_TRUE(fake_auto_add_dir_.CreateUniqueTempDir());
    } else {
      ASSERT_TRUE(fake_auto_add_dir_.Delete());
    }
  }

 private:
  base::ScopedTempDir fake_auto_add_dir_;
};

class TestITunesFileUtil : public ITunesFileUtil {
 public:
  explicit TestITunesFileUtil(MediaPathFilter* media_path_filter,
                              ITunesDataProvider* data_provider)
      : ITunesFileUtil(media_path_filter),
        data_provider_(data_provider) {
  }
  virtual ~TestITunesFileUtil() {}

 private:
  virtual ITunesDataProvider* GetDataProvider() OVERRIDE {
    return data_provider_;
  }

  ITunesDataProvider* data_provider_;
};

class TestMediaFileSystemBackend : public MediaFileSystemBackend {
 public:
  TestMediaFileSystemBackend(const base::FilePath& profile_path,
                             ITunesFileUtil* itunes_file_util)
      : MediaFileSystemBackend(
            profile_path,
            MediaFileSystemBackend::MediaTaskRunner().get()),
        test_file_util_(itunes_file_util) {}

  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) OVERRIDE {
    if (type != storage::kFileSystemTypeItunes)
      return NULL;

    return test_file_util_.get();
  }

 private:
  scoped_ptr<storage::AsyncFileUtil> test_file_util_;
};

class ItunesFileUtilTest : public testing::Test {
 public:
  ItunesFileUtilTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }
  virtual ~ItunesFileUtilTest() {}

  void SetUpDataProvider() {
    ASSERT_TRUE(fake_library_dir_.CreateUniqueTempDir());
    ASSERT_EQ(
        0,
        base::WriteFile(
            fake_library_dir_.path().AppendASCII(kITunesLibraryXML),
            NULL,
            0));

    itunes_data_provider_.reset(
        new TestITunesDataProvider(fake_library_dir_.path()));
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(profile_dir_.CreateUniqueTempDir());
    ImportedMediaGalleryRegistry::GetInstance()->Initialize();

    scoped_refptr<storage::SpecialStoragePolicy> storage_policy =
        new content::MockSpecialStoragePolicy();

    // Initialize fake ItunesDataProvider on media task runner thread.
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&ItunesFileUtilTest::SetUpDataProvider,
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
        new TestITunesFileUtil(media_path_filter_.get(),
                               itunes_data_provider_.get())));

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
    virtual_path = virtual_path.AppendASCII("itunes");
    virtual_path = virtual_path.AppendASCII(path);
    return file_system_context_->CreateCrackedFileSystemURL(
        GURL("http://www.example.com"),
        storage::kFileSystemTypeItunes,
        virtual_path);
  }

  storage::FileSystemOperationRunner* operation_runner() const {
    return file_system_context_->operation_runner();
  }

  scoped_refptr<storage::FileSystemContext> file_system_context() const {
    return file_system_context_;
  }

  TestITunesDataProvider* data_provider() const {
    return itunes_data_provider_.get();
  }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;

  base::ScopedTempDir profile_dir_;
  base::ScopedTempDir fake_library_dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_ptr<MediaPathFilter> media_path_filter_;
  scoped_ptr<TestITunesDataProvider> itunes_data_provider_;

  DISALLOW_COPY_AND_ASSIGN(ItunesFileUtilTest);
};

TEST_F(ItunesFileUtilTest, RootContents) {
  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL("");
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());

  EXPECT_FALSE(contents.front().is_directory);
  EXPECT_TRUE(contents.back().is_directory);

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kITunesLibraryXML).value(),
            contents.front().name);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kITunesMediaDir).value(),
            contents.back().name);
}

TEST_F(ItunesFileUtilTest, ItunesMediaDirectoryContentsNoAutoAdd) {
  data_provider()->SetProvideAutoAddDir(false);

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(kITunesMediaDir);
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kITunesMusicDir).value(),
            contents.back().name);
}

TEST_F(ItunesFileUtilTest, ItunesMediaDirectoryContentsAutoAdd) {
  data_provider()->SetProvideAutoAddDir(true);

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(kITunesMediaDir);
  bool completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);

  ASSERT_TRUE(completed);
  ASSERT_EQ(2u, contents.size());

  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_TRUE(contents.back().is_directory);

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kITunesAutoAddDir).value(),
            contents.front().name);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kITunesMusicDir).value(),
            contents.back().name);
}

TEST_F(ItunesFileUtilTest, ItunesAutoAddDirEnumerate) {
  data_provider()->SetProvideAutoAddDir(true);
  ASSERT_EQ(0, base::WriteFile(
      data_provider()->auto_add_path().AppendASCII("baz.ogg"), NULL, 0));

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(
      std::string(kITunesMediaDir) + "/" + kITunesAutoAddDir);
  bool completed = false;

  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());
  EXPECT_FALSE(contents.front().is_directory);
  EXPECT_EQ(base::FilePath().AppendASCII("baz.ogg").value(),
            contents.front().name);
}

TEST_F(ItunesFileUtilTest, ItunesAutoAddDirEnumerateNested) {
  data_provider()->SetProvideAutoAddDir(true);
  base::FilePath nested_dir =
      data_provider()->auto_add_path().AppendASCII("foo").AppendASCII("bar");
  ASSERT_TRUE(base::CreateDirectory(nested_dir));
  ASSERT_EQ(0,
            base::WriteFile(nested_dir.AppendASCII("baz.ogg"), NULL, 0));

  FileSystemOperation::FileEntryList contents;
  FileSystemURL url = CreateURL(
      std::string(kITunesMediaDir) + "/" + kITunesAutoAddDir);
  bool completed = false;

  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_EQ(base::FilePath().AppendASCII("foo").value(), contents.front().name);

  contents.clear();
  url = CreateURL(
      std::string(kITunesMediaDir) + "/" + kITunesAutoAddDir + "/foo");
  completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());
  EXPECT_TRUE(contents.front().is_directory);
  EXPECT_EQ(base::FilePath().AppendASCII("bar").value(), contents.front().name);

  contents.clear();
  url = CreateURL(
      std::string(kITunesMediaDir) + "/" + kITunesAutoAddDir + "/foo/bar");
  completed = false;
  ReadDirectoryTestHelper(operation_runner(), url, &contents, &completed);
  ASSERT_TRUE(completed);
  ASSERT_EQ(1u, contents.size());
  EXPECT_FALSE(contents.front().is_directory);
  EXPECT_EQ(base::FilePath().AppendASCII("baz.ogg").value(),
            contents.front().name);
}

}  // namespace itunes
