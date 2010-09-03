// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/file_system_backend.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/file_system/file_system_backend_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

namespace {
int kDummyRequestId = 42;
int kFileOperationStatusNotSet = 0;
int kFileOperationSucceeded = 1;
}

bool FileExists(FilePath path) {
  return file_util::PathExists(path) && !file_util::DirectoryExists(path);
}

class MockClient: public FileSystemBackendClient {
 public:
  MockClient()
      : status_(kFileOperationStatusNotSet) { }

  ~MockClient() {}

  void DidFail(WebKit::WebFileError status, int) {
    status_ = status;
  }

  void DidSucceed(int) {
    status_ = kFileOperationSucceeded;
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& info, int) {
    info_ = info;
  }

  void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more, int) {
    entries_ = entries;
  }

  // Helpers for testing.
  int status() const { return status_; }
  const base::PlatformFileInfo& info() const { return info_; }
  const std::vector<base::file_util_proxy::Entry>& entries() const {
    return entries_;
  }

 private:
  int status_;
  base::PlatformFileInfo info_;
  std::vector<base::file_util_proxy::Entry> entries_;
};

class FileSystemBackendTest : public testing::Test {
 public:
     FileSystemBackendTest()
         : ui_thread_(ChromeThread::UI, &loop_),
           file_thread_(ChromeThread::FILE, &loop_) {
       base_.CreateUniqueTempDir();
     }

 protected:
  virtual void SetUp() {
    mock_client_.reset(new MockClient());
    backend_.reset(new FileSystemBackend());
    backend_->set_client(mock_client_.get());
    ASSERT_TRUE(base_.IsValid());
  }

  scoped_ptr<FileSystemBackend> backend_;
  scoped_ptr<MockClient> mock_client_;
  MessageLoop loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;
  // Common temp base for all non-existing file/dir path test cases.
  // This is in case a dir on test machine exists. It's better to
  // create temp and then create non-existing file paths under it.
  ScopedTempDir base_;

 private:

  DISALLOW_COPY_AND_ASSIGN(FileSystemBackendTest);
};

TEST_F(FileSystemBackendTest, TestMoveFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  backend_->Move(src, dest, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestMoveFailureContainsPath) {
  FilePath file_under_base = base_.path().Append(FILE_PATH_LITERAL("b"));
  backend_->Move(base_.path(), file_under_base, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  backend_->Move(base_.path(), dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath nonexisting(base_.path().Append(FILE_PATH_LITERAL("DontExistDir")));
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("DontExistFile"));

  backend_->Move(src_dir.path(), nonexisting_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestMoveSuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);
  backend_->Move(src_file, dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(dest_file));
}

TEST_F(FileSystemBackendTest, TestMoveSuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));
  backend_->Move(src_file, dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(dest_file));
}

TEST_F(FileSystemBackendTest, TestCopyFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  backend_->Copy(src, dest, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestCopyFailureContainsPath) {
  FilePath file_under_base = base_.path().Append(FILE_PATH_LITERAL("b"));
  backend_->Copy(base_.path(), file_under_base, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dir.path(), &dest_file);

  backend_->Copy(base_.path(), dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath src_dir = dir.path();

  FilePath nonexisting(base_.path().Append(FILE_PATH_LITERAL("DontExistDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("DontExistFile"));

  backend_->Copy(src_dir, nonexisting_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestCopySuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);
  backend_->Copy(src_file, dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(dest_file));
}

TEST_F(FileSystemBackendTest, TestCopySuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));
  backend_->Copy(src_file, dest_file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(dest_file));
}

TEST_F(FileSystemBackendTest, TestCopySuccessSrcDir) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  backend_->Copy(src_dir.path(), dest_dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
}

TEST_F(FileSystemBackendTest, TestCopyDestParentExistsSuccess) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  backend_->Copy(src_file, dest_dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);

  FilePath src_filename(src_file.BaseName());
  EXPECT_TRUE(FileExists(dest_dir.path().Append(src_filename)));
}

TEST_F(FileSystemBackendTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;

  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  backend_->CreateFile(file, true, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);

  backend_->CreateFile(file, false, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(file));
}

TEST_F(FileSystemBackendTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  backend_->CreateFile(file, true, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(FileExists(file));
}

TEST_F(FileSystemBackendTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  backend_->CreateFile(file, false, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
}

TEST_F(FileSystemBackendTest, TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("DirDoesntExist")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("FileDoesntExist"));
  backend_->CreateDirectory(nonexisting_file, false, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  backend_->CreateDirectory(base_.path(), true, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  backend_->CreateDirectory(file, true, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  backend_->CreateDirectory(dir.path(), false, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FILE_PATH_LITERAL("nonexistingdir"));
  backend_->CreateDirectory(nonexisting_dir_path, false, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemBackendTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath nonexisting_dir_path(dir.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));
  backend_->CreateDirectory(nonexisting_dir_path, true, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
}

TEST_F(FileSystemBackendTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(base_.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));
  backend_->GetMetadata(nonexisting_dir_path, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);

  backend_->FileExists(nonexisting_dir_path, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  backend_->DirectoryExists(nonexisting_dir_path, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestExistsAndMetadataSuccess) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  backend_->DirectoryExists(dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);

  backend_->GetMetadata(dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_TRUE(mock_client_->info().is_directory);

  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  backend_->FileExists(file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);

  backend_->GetMetadata(file, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_FALSE(mock_client_->info().is_directory);
}

TEST_F(FileSystemBackendTest, TestReadDirFailure) {
  // Path doesn't exists
    FilePath nonexisting_dir_path(base_.path().Append(
        FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  backend_->ReadDirectory(nonexisting_dir_path, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);

  // File exists.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  backend_->ReadDirectory(file, kDummyRequestId);
  loop_.RunAllPending();
  // crbug.com/54309 to change the error code.
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
}

TEST_F(FileSystemBackendTest, TestReadDirSuccess) {
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify reading parent_dir.
  ScopedTempDir parent_dir;
  ASSERT_TRUE(parent_dir.CreateUniqueTempDir());
  FilePath child_file;
  file_util::CreateTemporaryFileInDir(parent_dir.path(), &child_file);
  FilePath child_dir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(parent_dir.path(),
      FILE_PATH_LITERAL("child_dir"), &child_dir));

  backend_->ReadDirectory(parent_dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationStatusNotSet);
  EXPECT_EQ(mock_client_->entries().size(), 2u);
  for (size_t i = 0; i < mock_client_->entries().size(); ++i) {
    if (mock_client_->entries()[i].is_directory)
      EXPECT_EQ(child_dir.BaseName().value(), mock_client_->entries()[i].name);
    else
      EXPECT_EQ(child_file.BaseName().value(), mock_client_->entries()[i].name);
  }
}

TEST_F(FileSystemBackendTest, TestRemoveFailure) {
  // Path doesn't exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  backend_->Remove(nonexisting, kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);

  // By spec recursive is always false. Non-empty dir should fail.
  //      parent_dir
  //       |       |
  //  child_dir  child_file
  // Verify deleting parent_dir.
  ScopedTempDir parent_dir;
  ASSERT_TRUE(parent_dir.CreateUniqueTempDir());
  FilePath child_file;
  file_util::CreateTemporaryFileInDir(parent_dir.path(), &child_file);
  FilePath child_dir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(parent_dir.path(),
      FILE_PATH_LITERAL("child_dir"), &child_dir));
  backend_->Remove(parent_dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
}

TEST_F(FileSystemBackendTest, TestRemoveSuccess) {
  ScopedTempDir empty_dir;
  ASSERT_TRUE(empty_dir.CreateUniqueTempDir());
  EXPECT_TRUE(file_util::DirectoryExists(empty_dir.path()));
  backend_->Remove(empty_dir.path(), kDummyRequestId);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), kFileOperationSucceeded);
  EXPECT_FALSE(file_util::DirectoryExists(empty_dir.path()));
}

