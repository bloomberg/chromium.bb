// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fileapi/provider_async_file_util.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_context.h"
#include "extensions/browser/extension_registry.h"
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
const char kFileSystemId[] = "testing-file-system";

// Logs callbacks invocations on the tested operations.
// TODO(mtomasz): Store and verify more arguments, once the operations return
// anything else than just an error.
class EventLogger {
 public:
  EventLogger() {}
  virtual ~EventLogger() {}

  void OnStatus(base::File::Error error) {
    result_.reset(new base::File::Error(error));
  }

  void OnCreateOrOpen(base::File file,
                      const base::Closure& on_close_callback) {
    if (file.IsValid())
      result_.reset(new base::File::Error(base::File::FILE_OK));

    result_.reset(new base::File::Error(file.error_details()));
  }

  void OnEnsureFileExists(base::File::Error error, bool created) {
    result_.reset(new base::File::Error(error));
  }

  void OnGetFileInfo(base::File::Error error,
                     const base::File::Info& file_info) {
    result_.reset(new base::File::Error(error));
  }

  void OnReadDirectory(base::File::Error error,
                       const storage::AsyncFileUtil::EntryList& file_list,
                       bool has_more) {
    result_.reset(new base::File::Error(error));
  }

  void OnCreateSnapshotFile(
      base::File::Error error,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<storage::ShareableFileReference>& file_ref) {
    result_.reset(new base::File::Error(error));
  }

  void OnCopyFileProgress(int64 size) {}

  base::File::Error* result() { return result_.get(); }

 private:
  scoped_ptr<base::File::Error> result_;
  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

// Creates a cracked FileSystemURL for tests.
storage::FileSystemURL CreateFileSystemURL(const std::string& mount_point_name,
                                           const base::FilePath& file_path) {
  const std::string origin = std::string("chrome-extension://") + kExtensionId;
  const storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  return mount_points->CreateCrackedFileSystemURL(
      GURL(origin),
      storage::kFileSystemTypeExternal,
      base::FilePath::FromUTF8Unsafe(mount_point_name).Append(file_path));
}

// Creates a Service instance. Used to be able to destroy the service in
// TearDown().
KeyedService* CreateService(content::BrowserContext* context) {
  return new Service(Profile::FromBrowserContext(context),
                     extensions::ExtensionRegistry::Get(context));
}

}  // namespace

// Tests in this file are very lightweight and just test integration between
// AsyncFileUtil and ProvideFileSystemInterface. Currently it tests if not
// implemented operations return a correct error code. For not allowed
// operations it is FILE_ERROR_ACCESS_DENIED, and for not implemented the error
// is FILE_ERROR_INVALID_OPERATION.
class FileSystemProviderProviderAsyncFileUtilTest : public testing::Test {
 protected:
  FileSystemProviderProviderAsyncFileUtilTest() {}
  virtual ~FileSystemProviderProviderAsyncFileUtilTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");
    async_file_util_.reset(new internal::ProviderAsyncFileUtil);

    file_system_context_ =
        content::CreateFileSystemContextForTesting(NULL, data_dir_.path());

    ServiceFactory::GetInstance()->SetTestingFactory(profile_, &CreateService);
    Service* service = Service::Get(profile_);  // Owned by its factory.
    service->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));

    const bool result = service->MountFileSystem(kExtensionId,
                                                 kFileSystemId,
                                                 "Testing File System",
                                                 false /* writable */);
    ASSERT_TRUE(result);
    const ProvidedFileSystemInfo& file_system_info =
        service->GetProvidedFileSystem(kExtensionId, kFileSystemId)
            ->GetFileSystemInfo();
    const std::string mount_point_name =
        file_system_info.mount_path().BaseName().AsUTF8Unsafe();

    file_url_ =
        CreateFileSystemURL(mount_point_name,
                            base::FilePath::FromUTF8Unsafe(
                                kFakeFilePath + 1 /* No leading slash. */));
    ASSERT_TRUE(file_url_.is_valid());
    directory_url_ = CreateFileSystemURL(
        mount_point_name, base::FilePath::FromUTF8Unsafe("hello"));
    ASSERT_TRUE(directory_url_.is_valid());
    root_url_ = CreateFileSystemURL(mount_point_name, base::FilePath());
    ASSERT_TRUE(root_url_.is_valid());
  }

  virtual void TearDown() OVERRIDE {
    // Setting the testing factory to NULL will destroy the created service
    // associated with the testing profile.
    ServiceFactory::GetInstance()->SetTestingFactory(profile_, NULL);
  }

  scoped_ptr<storage::FileSystemOperationContext> CreateOperationContext() {
    return make_scoped_ptr(
        new storage::FileSystemOperationContext(file_system_context_.get()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir data_dir_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;  // Owned by TestingProfileManager.
  scoped_ptr<storage::AsyncFileUtil> async_file_util_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  storage::FileSystemURL file_url_;
  storage::FileSystemURL directory_url_;
  storage::FileSystemURL root_url_;
};

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Create) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_CREATE,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_CreateAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_CREATE_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_OpenAlways) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN_ALWAYS,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest,
       CreateOrOpen_OpenTruncated) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN_TRUNCATED,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateOrOpen_Open) {
  EventLogger logger;

  async_file_util_->CreateOrOpen(
      CreateOperationContext(),
      file_url_,
      base::File::FLAG_OPEN,
      base::Bind(&EventLogger::OnCreateOrOpen, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, EnsureFileExists) {
  EventLogger logger;

  async_file_util_->EnsureFileExists(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnEnsureFileExists, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateDirectory) {
  EventLogger logger;

  async_file_util_->CreateDirectory(
      CreateOperationContext(),
      directory_url_,
      false,  // exclusive
      false,  // recursive
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, GetFileInfo) {
  EventLogger logger;

  async_file_util_->GetFileInfo(
      CreateOperationContext(),
      root_url_,
      base::Bind(&EventLogger::OnGetFileInfo, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, ReadDirectory) {
  EventLogger logger;

  async_file_util_->ReadDirectory(
      CreateOperationContext(),
      root_url_,
      base::Bind(&EventLogger::OnReadDirectory, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Touch) {
  EventLogger logger;

  async_file_util_->Touch(
      CreateOperationContext(),
      file_url_,
      base::Time(),  // last_modified_time
      base::Time(),  // last_access_time
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, Truncate) {
  EventLogger logger;

  async_file_util_->Truncate(
      CreateOperationContext(),
      file_url_,
      0,  // length
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyFileLocal) {
  EventLogger logger;

  async_file_util_->CopyFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      storage::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnCopyFileProgress, base::Unretained(&logger)),
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, MoveFileLocal) {
  EventLogger logger;

  async_file_util_->MoveFileLocal(
      CreateOperationContext(),
      file_url_,  // src_url
      file_url_,  // dst_url
      storage::FileSystemOperation::OPTION_NONE,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CopyInForeignFile) {
  EventLogger logger;

  async_file_util_->CopyInForeignFile(
      CreateOperationContext(),
      base::FilePath(),  // src_file_path
      file_url_,         // dst_url
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_ACCESS_DENIED, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteFile) {
  EventLogger logger;

  async_file_util_->DeleteFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteDirectory) {
  EventLogger logger;

  async_file_util_->DeleteDirectory(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, DeleteRecursively) {
  EventLogger logger;

  async_file_util_->DeleteRecursively(
      CreateOperationContext(),
      directory_url_,
      base::Bind(&EventLogger::OnStatus, base::Unretained(&logger)));
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_OK, *logger.result());
}

TEST_F(FileSystemProviderProviderAsyncFileUtilTest, CreateSnapshotFile) {
  EventLogger logger;

  async_file_util_->CreateSnapshotFile(
      CreateOperationContext(),
      file_url_,
      base::Bind(&EventLogger::OnCreateSnapshotFile,
                 base::Unretained(&logger)));

  ASSERT_TRUE(logger.result());
  EXPECT_EQ(base::File::FILE_ERROR_INVALID_OPERATION, *logger.result());
}

}  // namespace file_system_provider
}  // namespace chromeos
