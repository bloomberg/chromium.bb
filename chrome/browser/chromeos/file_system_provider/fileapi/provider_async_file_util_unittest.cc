// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/async_file_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const int kFileSystemId = 1;

// Logs callbacks invocations on the tested operations.
// TODO(mtomasz): Store and verify more arguments, once the operations return
// anything else than just an error.
class EventLogger {
 public:
  EventLogger() : weak_ptr_factory_(this) {}
  virtual ~EventLogger() {}

  void OnStatus(base::File::Error error) {
    error_.reset(new base::File::Error(error));
  }

  void OnCreateOrOpen(base::File::Error error,
                      base::PassPlatformFile platform_file,
                      const base::Closure& on_close_callback) {
    error_.reset(new base::File::Error(error));
  }

  void OnEnsureFileExists(base::File::Error error, bool created) {
    error_.reset(new base::File::Error(error));
  }

  void OnGetFileInfo(base::File::Error error,
                     const base::File::Info& file_info) {
    error_.reset(new base::File::Error(error));
  }

  void OnReadDirectory(base::File::Error error,
                       const fileapi::AsyncFileUtil::EntryList& file_list,
                       bool has_more) {
    error_.reset(new base::File::Error(error));
  }

  void OnCreateSnapshotFile(
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
    error_.reset(new base::File::Error(error));
  }

  void OnCopyFileProgress(int64 size) {}

  base::WeakPtr<EventLogger> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  base::File::Error* error() { return error_.get(); }

 private:
  scoped_ptr<base::File::Error> error_;
  base::WeakPtrFactory<EventLogger> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Registers an external mount point, and removes it once the object gets out
// of scope. To ensure that creating the mount point succeeded, call is_valid().
class ScopedExternalMountPoint {
 public:
  ScopedExternalMountPoint(const std::string& mount_point_name,
                           const base::FilePath& mount_path,
                           fileapi::FileSystemType type)
      : mount_point_name_(mount_point_name) {
    fileapi::ExternalMountPoints* const mount_points =
        fileapi::ExternalMountPoints::GetSystemInstance();
    DCHECK(mount_points);
    is_valid_ =
        mount_points->RegisterFileSystem(mount_point_name,
                                         fileapi::kFileSystemTypeProvided,
                                         fileapi::FileSystemMountOption(),
                                         mount_path);
  }

  virtual ~ScopedExternalMountPoint() {
    if (!is_valid_)
      return;

    // If successfully registered in the constructor, then unregister.
    fileapi::ExternalMountPoints* const mount_points =
        fileapi::ExternalMountPoints::GetSystemInstance();
    DCHECK(mount_points);
    mount_points->RevokeFileSystem(mount_point_name_);
  }

  bool is_valid() { return is_valid_; }

 private:
  const std::string mount_point_name_;
  bool is_valid_;
};

// Creates a cracked FileSystemURL for tests.
fileapi::FileSystemURL CreateFileSystemURL(const std::string& mount_point_name,
                                           const base::FilePath& file_path) {
  const std::string origin = std::string("chrome-extension://") + kExtensionId;
  const fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateCrackedFileSystemURL(
      GURL(origin),
      fileapi::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(mount_point_name).Append(file_path));
}

}  // namespace

// Tests in this file are very lightweight and just test integration between
// AsyncFileUtil and ProvideFileSystemInterface. Currently it tests if not
// implemented operations return a correct error code. For not allowed
// operations it is FILE_ERROR_SECURITY, and for not implemented the error is
// FILE_ERROR_NOT_FOUND.
class FileSystemProviderProviderAsyncFileUtilTest : public testing::Test {
 protected:
  FileSystemProviderProviderAsyncFileUtilTest() {}
  virtual ~FileSystemProviderProviderAsyncFileUtilTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_.reset(new TestingProfile);
    async_file_util_.reset(new internal::ProviderAsyncFileUtil);
    const base::FilePath mount_path =
        util::GetMountPath(profile_.get(), kExtensionId, kFileSystemId);
    file_system_context_ =
        content::CreateFileSystemContextForTesting(NULL, data_dir_.path());

    const std::string mount_point_name = mount_path.BaseName().AsUTF8Unsafe();
    mount_point_.reset(new ScopedExternalMountPoint(
        mount_point_name, mount_path, fileapi::kFileSystemTypeProvided));
    ASSERT_TRUE(mount_point_->is_valid());

    file_url_ = CreateFileSystemURL(
        mount_point_name, base::FilePath::FromUTF8Unsafe("hello/world.txt"));
    ASSERT_TRUE(file_url_.is_valid());
    directory_url_ = CreateFileSystemURL(
        mount_point_name, base::FilePath::FromUTF8Unsafe("hello"));
    ASSERT_TRUE(directory_url_.is_valid());
    root_url_ = CreateFileSystemURL(mount_point_name, base::FilePath());
    ASSERT_TRUE(root_url_.is_valid());
  }

  scoped_ptr<fileapi::FileSystemOperationContext> CreateOperationContext() {
    return make_scoped_ptr(
        new fileapi::FileSystemOperationContext(file_system_context_.get()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir data_dir_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<fileapi::AsyncFileUtil> async_file_util_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;
  scoped_ptr<ScopedExternalMountPoint> mount_point_;
  fileapi::FileSystemURL file_url_;
  fileapi::FileSystemURL directory_url_;
  fileapi::FileSystemURL root_url_;
};

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Create) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::PLATFORM_FILE_CREATE,
      base::Bind(&EventLogger::OnCreateOrOpen, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_CreateAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::PLATFORM_FILE_CREATE_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_OpenAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::PLATFORM_FILE_OPEN_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       CreateOrOpen_OpenTruncated) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::PLATFORM_FILE_OPEN_TRUNCATED,
      base::Bind(&EventLogger::OnCreateOrOpen, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Open) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::PLATFORM_FILE_OPEN,
      base::Bind(&EventLogger::OnCreateOrOpen, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, EnsureFileExists) {
  EventLogger logger;

  async_file_util_->EnsureFileExists(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnEnsureFileExists, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateDirectory) {
  EventLogger logger;

  async_file_util_->CreateDirectory(
      CreateOperationContext(),
      directory_url_,
      false,  // exclusive
      false,  // recursive
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, GetFileInfo) {
  EventLogger logger;

  async_file_util_->GetFileInfo(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnGetFileInfo, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, ReadDirectory) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(),
      root_url_,
      base::Bind(&EventLogger::OnReadDirectory, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Touch) {
  EventLogger logger;

  async_file_util_->CreateDirectory(
      CreateOperationContext(),
      file_url_,
      false,  // exclusive
      false,  // recursive
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Truncate) {
  EventLogger logger;

  async_file_util_->Truncate(
      CreateOperationContext(),
      file_url_,
      0,  // length
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyFileLocal) {
  EventLogger logger;

  async_file_util_->CopyFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      fileapi::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnCopyFileProgress, logger.GetWeakPtr()),
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, MoveFileLocal) {
  EventLogger logger;

  async_file_util_->MoveFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      fileapi::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyInForeignFile) {
  EventLogger logger;

  async_file_util_->CopyInForeignFile(
      CreateOperationContext(),
      base::FilePath(),  // src_file_path
      file_url_,         // dst_url
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteFile) {
  EventLogger logger;

  async_file_util_->DeleteFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteDirectory) {
  EventLogger logger;

  async_file_util_->DeleteDirectory(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteRecursively) {
  EventLogger logger;

  async_file_util_->DeleteRecursively(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_SECURITY, *logger.error());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateSnapshotFile) {
  EventLogger logger;

  async_file_util_->CreateSnapshotFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnCreateSnapshotFile, logger.GetWeakPtr()));

  ASSERT_TRUE(logger.error());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, *logger.error());
}

}  // namespace file_system_provider
}  // namespace chromeos
