// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_manager.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/mock_download_file.h"
#include "content/public/browser/download_id.h"
#include "content/public/test/mock_download_manager.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;
using content::DownloadId;
using content::MockDownloadManager;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::StrEq;

namespace {

// MockDownloadManager with the addition of a mock callback used for testing.
class TestDownloadManager : public MockDownloadManager {
 public:
  MOCK_METHOD2(OnDownloadRenamed,
               void(int download_id, const FilePath& full_path));
 private:
  ~TestDownloadManager() {}
};

class MockDownloadFileFactory :
    public DownloadFileManager::DownloadFileFactory {

 public:
  MockDownloadFileFactory() {}
  virtual ~MockDownloadFileFactory() {}

  virtual content::DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      scoped_ptr<content::ByteStreamReader> stream,
      content::DownloadManager* download_manager,
      bool calculate_hash,
      const net::BoundNetLog& bound_net_log) OVERRIDE;

  MockDownloadFile* GetExistingFile(const DownloadId& id);

 private:
  std::map<DownloadId, MockDownloadFile*> files_;
};

content::DownloadFile* MockDownloadFileFactory::CreateFile(
    DownloadCreateInfo* info,
    scoped_ptr<content::ByteStreamReader> stream,
    content::DownloadManager* download_manager,
    bool calculate_hash,
    const net::BoundNetLog& bound_net_log) {
  DCHECK(files_.end() == files_.find(info->download_id));
  MockDownloadFile* created_file = new StrictMock<MockDownloadFile>();
  files_[info->download_id] = created_file;

  ON_CALL(*created_file, GetDownloadManager())
      .WillByDefault(Return(download_manager));
  EXPECT_CALL(*created_file, Initialize());

  return created_file;
}

MockDownloadFile* MockDownloadFileFactory::GetExistingFile(
    const DownloadId& id) {
  DCHECK(files_.find(id) != files_.end());
  return files_[id];
}

class MockDownloadRequestHandle : public DownloadRequestHandle {
 public:
  MockDownloadRequestHandle(content::DownloadManager* manager)
      : manager_(manager) {}

  virtual content::DownloadManager* GetDownloadManager() const OVERRIDE;

 private:

  content::DownloadManager* manager_;
};

content::DownloadManager* MockDownloadRequestHandle::GetDownloadManager()
    const {
  return manager_;
}

void NullCallback() { }

}  // namespace

class DownloadFileManagerTest : public testing::Test {
 public:
  // State of renamed file. Used with RenameFile().
  enum RenameFileState {
    IN_PROGRESS,
    COMPLETE
  };

  // Whether to overwrite the target filename in RenameFile().
  enum RenameFileOverwrite {
    OVERWRITE,
    DONT_OVERWRITE
  };

  static const char* kTestData1;
  static const char* kTestData2;
  static const char* kTestData3;
  static const char* kTestData4;
  static const char* kTestData5;
  static const char* kTestData6;
  static const int32 kDummyDownloadId;
  static const int32 kDummyDownloadId2;
  static const int kDummyChildId;
  static const int kDummyRequestId;

  // We need a UI |BrowserThread| in order to destruct |download_manager_|,
  // which has trait |BrowserThread::DeleteOnUIThread|.  Without this,
  // calling Release() on |download_manager_| won't ever result in its
  // destructor being called and we get a leak.
  DownloadFileManagerTest()
      : last_reason_(content::DOWNLOAD_INTERRUPT_REASON_NONE),
        ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_) {
  }

  ~DownloadFileManagerTest() {
  }

  virtual void SetUp() {
    download_manager_ = new TestDownloadManager();
    request_handle_.reset(new MockDownloadRequestHandle(download_manager_));
    download_file_factory_ = new MockDownloadFileFactory;
    download_file_manager_ = new DownloadFileManager(download_file_factory_);
  }

  virtual void TearDown() {
    // When a DownloadManager's reference count drops to 0, it is not
    // deleted immediately. Instead, a task is posted to the UI thread's
    // message loop to delete it.
    // So, drop the reference count to 0 and run the message loop once
    // to ensure that all resources are cleaned up before the test exits.
    download_manager_ = NULL;
    ui_thread_.message_loop()->RunAllPending();
  }

  void ProcessAllPendingMessages() {
    loop_.RunAllPending();
  }

  // Clears all gmock expectations for the download file |id| and the manager.
  void ClearExpectations(DownloadId id) {
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    Mock::VerifyAndClearExpectations(file);
    Mock::VerifyAndClearExpectations(download_manager_);
  }

  void OnDownloadFileCreated(content::DownloadInterruptReason reason) {
    last_reason_ = reason;
  }

  // Create a download item on the DFM.
  // |info| is the information needed to create a new download file.
  // |id| is the download ID of the new download file.
  void CreateDownloadFile(scoped_ptr<DownloadCreateInfo> info) {
    // Mostly null out args; they'll be passed to MockDownloadFileFactory
    // to be ignored anyway.
    download_file_manager_->CreateDownloadFile(
        info.Pass(), scoped_ptr<content::ByteStreamReader>(),
        download_manager_, true, net::BoundNetLog(),
        base::Bind(&DownloadFileManagerTest::OnDownloadFileCreated,
                   // The test jig will outlive all download files.
                   base::Unretained(this)));

    // Anything that isn't DOWNLOAD_INTERRUPT_REASON_NONE.
    last_reason_ = content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
    ProcessAllPendingMessages();
    EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE, last_reason_);
  }

  // Renames the download file.
  // |id|           is the download ID of the download file.
  // |new_path|     is the new file path.
  // |unique_path|  is the actual path that the download file will be
  //                renamed to. If there is an existing file at
  //                |new_path| and |replace| is false, then |new_path|
  //                will be uniquified.
  // |rename_error| is the error to inject.  For no error,
  //                use content::DOWNLOAD_INTERRUPT_REASON_NONE.
  // |state|        whether we are renaming an in-progress download or a
  //                completed download.
  // |should_overwrite| indicates whether to replace or uniquify the file.
  void RenameFile(const DownloadId& id,
                  const FilePath& new_path,
                  const FilePath& unique_path,
                  content::DownloadInterruptReason rename_error,
                  RenameFileState state,
                  RenameFileOverwrite should_overwrite) {
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);

    EXPECT_CALL(*file, Rename(unique_path))
        .Times(1)
        .WillOnce(Return(rename_error));

    if (rename_error != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
      EXPECT_CALL(*file, BytesSoFar())
          .Times(AtLeast(1))
          .WillRepeatedly(Return(byte_count_[id]));
      EXPECT_CALL(*file, GetHashState())
          .Times(AtLeast(1));
      EXPECT_CALL(*file, GetDownloadManager())
          .Times(AtLeast(1));
    }

    download_file_manager_->RenameDownloadFile(
        id, new_path, (should_overwrite == OVERWRITE),
        base::Bind(&TestDownloadManager::OnDownloadRenamed,
                   download_manager_, id.local()));

    if (rename_error != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
      EXPECT_CALL(*download_manager_,
                  OnDownloadInterrupted(
                      id.local(),
                      byte_count_[id],
                      "",
                      rename_error));
      EXPECT_CALL(*download_manager_,
                  OnDownloadRenamed(id.local(), FilePath()));
      ProcessAllPendingMessages();
      ++error_count_[id];
    } else {
      EXPECT_CALL(*download_manager_,
                  OnDownloadRenamed(id.local(), unique_path));
      ProcessAllPendingMessages();
    }
  }

  void Complete(DownloadId id) {
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);

    EXPECT_CALL(*file, AnnotateWithSourceInformation())
        .WillOnce(Return());
    EXPECT_CALL(*file, Detach())
        .WillOnce(Return());
    int num_downloads = download_file_manager_->NumberOfActiveDownloads();
    EXPECT_LT(0, num_downloads);
    download_file_manager_->CompleteDownload(id, base::Bind(NullCallback));
    EXPECT_EQ(num_downloads - 1,
              download_file_manager_->NumberOfActiveDownloads());
    EXPECT_EQ(NULL, download_file_manager_->GetDownloadFile(id));
  }

  void CleanUp(DownloadId id) {
    // Expected calls:
    //  DownloadFileManager::CancelDownload
    //    DownloadFile::Cancel
    //    DownloadFileManager::EraseDownload
    //      if no more downloads:
    //        DownloadFileManager::StopUpdateTimer
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);

    EXPECT_CALL(*file, Cancel());

    download_file_manager_->CancelDownload(id);

    EXPECT_EQ(NULL, download_file_manager_->GetDownloadFile(id));
  }

 protected:
  scoped_refptr<TestDownloadManager> download_manager_;
  scoped_ptr<MockDownloadRequestHandle> request_handle_;
  MockDownloadFileFactory* download_file_factory_;
  scoped_refptr<DownloadFileManager> download_file_manager_;

  // Error from creating download file.
  content::DownloadInterruptReason last_reason_;

  // Per-download statistics.
  std::map<DownloadId, int64> byte_count_;
  std::map<DownloadId, int> error_count_;

 private:
  MessageLoop loop_;

  // UI thread.
  BrowserThreadImpl ui_thread_;

  // File thread to satisfy debug checks in DownloadFile.
  BrowserThreadImpl file_thread_;
};

const char* DownloadFileManagerTest::kTestData1 =
    "Let's write some data to the file!\n";
const char* DownloadFileManagerTest::kTestData2 = "Writing more data.\n";
const char* DownloadFileManagerTest::kTestData3 = "Final line.";
const char* DownloadFileManagerTest::kTestData4 = "Writing, writing, writing\n";
const char* DownloadFileManagerTest::kTestData5 = "All we do is writing,\n";
const char* DownloadFileManagerTest::kTestData6 = "Rawhide!";

const int32 DownloadFileManagerTest::kDummyDownloadId = 23;
const int32 DownloadFileManagerTest::kDummyDownloadId2 = 77;
const int DownloadFileManagerTest::kDummyChildId = 3;
const int DownloadFileManagerTest::kDummyRequestId = 67;

TEST_F(DownloadFileManagerTest, Cancel) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;

  CreateDownloadFile(info.Pass());

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, Complete) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;

  CreateDownloadFile(info.Pass());

  Complete(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameInProgress) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  RenameFile(dummy_id, foo, foo, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             IN_PROGRESS, OVERWRITE);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameInProgressWithUniquification) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  FilePath unique_foo(foo.InsertBeforeExtension(FILE_PATH_LITERAL(" (1)")));
  ASSERT_EQ(0, file_util::WriteFile(foo, "", 0));
  RenameFile(dummy_id, foo, unique_foo,
             content::DOWNLOAD_INTERRUPT_REASON_NONE, IN_PROGRESS,
             DONT_OVERWRITE);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameInProgressWithError) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  RenameFile(dummy_id, foo, foo,
             content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG,
             IN_PROGRESS, OVERWRITE);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameWithUniquification) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  FilePath unique_foo(foo.InsertBeforeExtension(FILE_PATH_LITERAL(" (1)")));
  // Create a file at |foo|. Since we are specifying DONT_OVERWRITE,
  // RenameDownloadFile() should pick "foo (1).txt" instead of
  // overwriting this file.
  ASSERT_EQ(0, file_util::WriteFile(foo, "", 0));
  RenameFile(dummy_id, foo, unique_foo,
             content::DOWNLOAD_INTERRUPT_REASON_NONE, COMPLETE, DONT_OVERWRITE);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameTwice) {
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());

  FilePath crfoo(download_dir.path().Append(
      FILE_PATH_LITERAL("foo.txt.crdownload")));
  RenameFile(dummy_id, crfoo, crfoo, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             IN_PROGRESS, OVERWRITE);

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  RenameFile(dummy_id, foo, foo, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             COMPLETE, OVERWRITE);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, TwoDownloads) {
  // Same as StartDownload, at first.
  scoped_ptr<DownloadCreateInfo> info(new DownloadCreateInfo);
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  info->download_id = dummy_id;
  scoped_ptr<DownloadCreateInfo> info2(new DownloadCreateInfo);
  DownloadId dummy_id2(download_manager_.get(), kDummyDownloadId2);
  info2->download_id = dummy_id2;
  ScopedTempDir download_dir;
  ASSERT_TRUE(download_dir.CreateUniqueTempDir());

  CreateDownloadFile(info.Pass());
  CreateDownloadFile(info2.Pass());

  FilePath crbar(download_dir.path().Append(
      FILE_PATH_LITERAL("bar.txt.crdownload")));
  RenameFile(dummy_id2, crbar, crbar, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             IN_PROGRESS, OVERWRITE);

  FilePath crfoo(download_dir.path().Append(
      FILE_PATH_LITERAL("foo.txt.crdownload")));
  RenameFile(dummy_id, crfoo, crfoo, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             IN_PROGRESS, OVERWRITE);


  FilePath bar(download_dir.path().Append(FILE_PATH_LITERAL("bar.txt")));
  RenameFile(dummy_id2, bar, bar, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             COMPLETE, OVERWRITE);

  CleanUp(dummy_id2);

  FilePath foo(download_dir.path().Append(FILE_PATH_LITERAL("foo.txt")));
  RenameFile(dummy_id, foo, foo, content::DOWNLOAD_INTERRUPT_REASON_NONE,
             COMPLETE, OVERWRITE);

  CleanUp(dummy_id);
}

// TODO(ahendrickson) -- A test for download manager shutdown.
// Expected call sequence:
//  OnDownloadManagerShutdown
//    DownloadFile::GetDownloadManager
//    DownloadFile::CancelDownloadRequest
//    DownloadFile::~DownloadFile
