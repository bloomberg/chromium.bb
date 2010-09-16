// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/chrome_file_system_operation.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/chrome_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"
#include "webkit/fileapi/file_system_operation_client.h"

namespace {
int kInvalidRequestId = -1;
int kFileOperationStatusNotSet = 0;
int kFileOperationSucceeded = 1;
}

using fileapi::FileSystemOperation;

bool FileExists(FilePath path) {
  return file_util::PathExists(path) && !file_util::DirectoryExists(path);
}

class MockClient: public fileapi::FileSystemOperationClient {
 public:
  MockClient()
      : status_(kFileOperationStatusNotSet),
        request_id_(kInvalidRequestId) {
  }

  ~MockClient() {}

  virtual void DidFail(WebKit::WebFileError status,
      FileSystemOperation* operation) {
    status_ = status;
    request_id_ =
        static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  }

  virtual void DidSucceed(FileSystemOperation* operation) {
    status_ = kFileOperationSucceeded;
    request_id_ =
        static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  }

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      FileSystemOperation* operation) {
    info_ = info;
    request_id_ =
        static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  }

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries,
      bool has_more,
      FileSystemOperation* operation) {
    entries_ = entries;
    request_id_ =
        static_cast<ChromeFileSystemOperation*>(operation)->request_id();
  }

  // Helpers for testing.
  int status() const { return status_; }
  int request_id() const { return request_id_; }
  const base::PlatformFileInfo& info() const { return info_; }
  const std::vector<base::file_util_proxy::Entry>& entries() const {
    return entries_;
  }

 private:
  int status_;
  int request_id_;
  base::PlatformFileInfo info_;
  std::vector<base::file_util_proxy::Entry> entries_;
};

class ChromeFileSystemOperationTest : public testing::Test {
 public:
  ChromeFileSystemOperationTest()
      : ui_thread_(ChromeThread::UI, &loop_),
        file_thread_(ChromeThread::FILE, &loop_) {
        base_.CreateUniqueTempDir();
     }

 protected:
  virtual void SetUp() {
    mock_client_.reset(new MockClient());
    current_request_id_ = kInvalidRequestId;
    ASSERT_TRUE(base_.IsValid());
  }

  // Returns a new operation pointer that is created each time it's called.
  // The pointer is owned by the test class.
  ChromeFileSystemOperation* operation() {
    current_request_id_ = base::RandInt(0, kint32max);
    operation_.reset(new ChromeFileSystemOperation(
      current_request_id_, mock_client_.get()));
    return operation_.get();
  }

  scoped_ptr<MockClient> mock_client_;
  int current_request_id_;

  MessageLoop loop_;
  ChromeThread ui_thread_;
  ChromeThread file_thread_;

  // Common temp base for all non-existing file/dir path test cases.
  // This is in case a dir on test machine exists. It's better to
  // create temp and then create non-existing file paths under it.
  ScopedTempDir base_;

 private:
  scoped_ptr<ChromeFileSystemOperation> operation_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFileSystemOperationTest);
};

TEST_F(ChromeFileSystemOperationTest, TestMoveFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  operation()->Move(src, dest);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}


TEST_F(ChromeFileSystemOperationTest, TestMoveFailureContainsPath) {
  FilePath file_under_base = base_.path().Append(FILE_PATH_LITERAL("b"));
  operation()->Move(base_.path(), file_under_base);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorInvalidModification, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestMoveFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Move(base_.path(), dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestMoveFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath nonexisting(base_.path().Append(FILE_PATH_LITERAL("DontExistDir")));
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("DontExistFile"));

  operation()->Move(src_dir.path(), nonexisting_file);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestMoveSuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Move(src_file, dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestMoveSuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Move(src_file, dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopyFailureSrcDoesntExist) {
  FilePath src(base_.path().Append(FILE_PATH_LITERAL("a")));
  FilePath dest(base_.path().Append(FILE_PATH_LITERAL("b")));
  operation()->Copy(src, dest);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopyFailureContainsPath) {
  FilePath file_under_base = base_.path().Append(FILE_PATH_LITERAL("b"));
  operation()->Copy(base_.path(), file_under_base);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorInvalidModification, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopyFailureSrcDirExistsDestFile) {
  // Src exists and is dir. Dest is a file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dir.path(), &dest_file);

  operation()->Copy(base_.path(), dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopyFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath src_dir = dir.path();

  FilePath nonexisting(base_.path().Append(FILE_PATH_LITERAL("DontExistDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("DontExistFile"));

  operation()->Copy(src_dir, nonexisting_file);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorNotFound);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopySuccessSrcFileAndOverwrite) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file;
  file_util::CreateTemporaryFileInDir(dest_dir.path(), &dest_file);

  operation()->Copy(src_file, dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopySuccessSrcFileAndNew) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());
  FilePath dest_file(dest_dir.path().Append(FILE_PATH_LITERAL("NewFile")));

  operation()->Copy(src_file, dest_file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(dest_file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopySuccessSrcDir) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  operation()->Copy(src_dir.path(), dest_dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCopyDestParentExistsSuccess) {
  ScopedTempDir src_dir;
  ASSERT_TRUE(src_dir.CreateUniqueTempDir());
  FilePath src_file;
  file_util::CreateTemporaryFileInDir(src_dir.path(), &src_file);

  ScopedTempDir dest_dir;
  ASSERT_TRUE(dest_dir.CreateUniqueTempDir());

  operation()->Copy(src_file, dest_dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  FilePath src_filename(src_file.BaseName());
  EXPECT_TRUE(FileExists(dest_dir.path().Append(src_filename)));
}

TEST_F(ChromeFileSystemOperationTest, TestCreateFileFailure) {
  // Already existing file and exclusive true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;

  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->CreateFile(file, true);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(), WebKit::WebFileErrorInvalidModification);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateFileSuccessFileExists) {
  // Already existing file and exclusive false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);

  operation()->CreateFile(file, false);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateFileSuccessExclusive) {
  // File doesn't exist but exclusive is true.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateFile(file, true);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(FileExists(file));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateFileSuccessFileDoesntExist) {
  // Non existing file.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file = dir.path().Append(FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateFile(file, false);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest,
       TestCreateDirFailureDestParentDoesntExist) {
  // Dest. parent path does not exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("DirDoesntExist")));
  file_util::EnsureEndsWithSeparator(&nonexisting);
  FilePath nonexisting_file = nonexisting.Append(
      FILE_PATH_LITERAL("FileDoesntExist"));
  operation()->CreateDirectory(nonexisting_file, false);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateDirFailureDirExists) {
  // Exclusive and dir existing at path.
  operation()->CreateDirectory(base_.path(), true);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorInvalidModification, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateDirFailureFileExists) {
  // Exclusive true and file existing at path.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->CreateDirectory(file, true);
  loop_.RunAllPending();
  EXPECT_EQ(mock_client_->status(),
            WebKit::WebFileErrorInvalidModification);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateDirSuccess) {
  // Dir exists and exclusive is false.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  operation()->CreateDirectory(dir.path(), false);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  // Dir doesn't exist.
  FilePath nonexisting_dir_path(FILE_PATH_LITERAL("nonexistingdir"));
  operation()->CreateDirectory(nonexisting_dir_path, false);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestCreateDirSuccessExclusive) {
  // Dir doesn't exist.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath nonexisting_dir_path(dir.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));

  operation()->CreateDirectory(nonexisting_dir_path, true);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(file_util::DirectoryExists(nonexisting_dir_path));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestExistsAndMetadataFailure) {
  FilePath nonexisting_dir_path(base_.path().Append(
      FILE_PATH_LITERAL("nonexistingdir")));
  operation()->GetMetadata(nonexisting_dir_path);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());

  operation()->FileExists(nonexisting_dir_path);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->DirectoryExists(nonexisting_dir_path);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestExistsAndMetadataSuccess) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());

  operation()->DirectoryExists(dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  operation()->GetMetadata(dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_TRUE(mock_client_->info().is_directory);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->FileExists(file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  operation()->GetMetadata(file);
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_FALSE(mock_client_->info().is_directory);
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestReadDirFailure) {
  // Path doesn't exists
    FilePath nonexisting_dir_path(base_.path().Append(
        FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting_dir_path);
  operation()->ReadDirectory(nonexisting_dir_path);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  // File exists.
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath file;
  file_util::CreateTemporaryFileInDir(dir.path(), &file);
  operation()->ReadDirectory(file);
  loop_.RunAllPending();
  // TODO(kkanetkar) crbug.com/54309 to change the error code.
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestReadDirSuccess) {
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

  operation()->ReadDirectory(parent_dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationStatusNotSet, mock_client_->status());
  EXPECT_EQ(2u, mock_client_->entries().size());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

  for (size_t i = 0; i < mock_client_->entries().size(); ++i) {
    if (mock_client_->entries()[i].is_directory)
      EXPECT_EQ(child_dir.BaseName().value(), mock_client_->entries()[i].name);
    else
      EXPECT_EQ(child_file.BaseName().value(), mock_client_->entries()[i].name);
  }
}

TEST_F(ChromeFileSystemOperationTest, TestRemoveFailure) {
  // Path doesn't exist.
  FilePath nonexisting(base_.path().Append(
      FILE_PATH_LITERAL("NonExistingDir")));
  file_util::EnsureEndsWithSeparator(&nonexisting);

  operation()->Remove(nonexisting);
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorNotFound, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());

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

  operation()->Remove(parent_dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(WebKit::WebFileErrorInvalidModification, mock_client_->status());
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}

TEST_F(ChromeFileSystemOperationTest, TestRemoveSuccess) {
  ScopedTempDir empty_dir;
  ASSERT_TRUE(empty_dir.CreateUniqueTempDir());
  EXPECT_TRUE(file_util::DirectoryExists(empty_dir.path()));

  operation()->Remove(empty_dir.path());
  loop_.RunAllPending();
  EXPECT_EQ(kFileOperationSucceeded, mock_client_->status());
  EXPECT_FALSE(file_util::DirectoryExists(empty_dir.path()));
  EXPECT_EQ(current_request_id_, mock_client_->request_id());
}
