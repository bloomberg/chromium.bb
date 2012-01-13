// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_manager.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/download_buffer.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_id.h"
#include "content/browser/download/download_id_factory.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/download/download_status_updater.h"
#include "content/browser/download/mock_download_file.h"
#include "content/browser/download/mock_download_manager.h"
#include "content/browser/download/mock_download_manager_delegate.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::StrEq;

namespace {

DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class MockDownloadFileFactory :
    public DownloadFileManager::DownloadFileFactory {

 public:
  MockDownloadFileFactory() {}
  virtual ~MockDownloadFileFactory() {}

  virtual content::DownloadFile* CreateFile(
      DownloadCreateInfo* info,
      const DownloadRequestHandle& request_handle,
      content::DownloadManager* download_manager,
      bool calculate_hash) OVERRIDE;

  MockDownloadFile* GetExistingFile(const DownloadId& id);

 private:
  std::map<DownloadId, MockDownloadFile*> files_;
};

content::DownloadFile* MockDownloadFileFactory::CreateFile(
    DownloadCreateInfo* info,
    const DownloadRequestHandle& request_handle,
    content::DownloadManager* download_manager,
    bool calculate_hash) {
  DCHECK(files_.end() == files_.find(info->download_id));
  MockDownloadFile* created_file = new MockDownloadFile();
  files_[info->download_id] = created_file;

  ON_CALL(*created_file, GetDownloadManager())
      .WillByDefault(Return(download_manager));

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

}  // namespace

class DownloadFileManagerTest : public testing::Test {
 public:
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
    : download_buffer_(new content::DownloadBuffer()),
      id_factory_(new DownloadIdFactory(kValidIdDomain)),
      ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE, &loop_) {
  }

  ~DownloadFileManagerTest() {
  }

  virtual void SetUp() {
    download_manager_ = new MockDownloadManager();
    request_handle_.reset(new MockDownloadRequestHandle(download_manager_));
    download_file_factory_ = new MockDownloadFileFactory;
    download_file_manager_ =
        new DownloadFileManager(NULL, download_file_factory_);
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

  // Start a download.
  // |info| is the information needed to create a new download file.
  // |id| is the download ID of the new download file.
  void StartDownload(DownloadCreateInfo* info, const DownloadId& id) {
    // Expected call sequence:
    //  StartDownload
    //    DownloadManager::CreateDownloadItem
    //    DownloadManagerDelegate::GenerateFileHash
    //    Process one message in the message loop
    //      CreateDownloadFile
    //        new MockDownloadFile, add to downloads_[id]
    //        DownloadRequestHandle::ResumeRequest
    //        StartUpdateTimer
    //        DownloadCreateInfo is destroyed
    //    Process one message in the message loop
    //        DownloadManager::StartDownload
    info->download_id = id;

    // Set expectations and return values.
    EXPECT_CALL(*download_manager_, CreateDownloadItem(info, _))
        .Times(1);
    EXPECT_CALL(*download_manager_, GenerateFileHash())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*download_manager_, StartDownload(id.local()));

    download_file_manager_->StartDownload(info, *request_handle_);
    ProcessAllPendingMessages();

    ClearExpectations(id);
  }

  // Creates a new |net::IOBuffer| of size |length| with |data|, and attaches
  // it to the download buffer.
  bool UpdateBuffer(const char* data, size_t length) {
    // We are passing ownership of this buffer to the download file manager.
    // NOTE: we are padding io_buffer with one extra character so that the
    // mock testing framework can treat it as a NULL-delimited string.
    net::IOBuffer* io_buffer = new net::IOBuffer(length + 1);
    // We need |AddRef()| because we do a |Release()| in |UpdateDownload()|.
    io_buffer->AddRef();
    memcpy(io_buffer->data(), data, length);
    io_buffer->data()[length] = 0;

    download_buffer_->AddData(io_buffer, length);

    return true;
  }

  // Updates the download file.
  // |id| is the download ID of the download file.
  // |data| is the buffer containing the data.
  // |length| is the length of the data buffer.
  // |error_to_insert| is the error to simulate.  For no error, use net::OK.
  void UpdateDownload(const DownloadId& id,
                      const char* data,
                      size_t length,
                      net::Error error_to_insert) {
    // Expected call sequence:
    //  Create DownloadBuffer
    //  UpdateDownload
    //    GetDownloadFile
    //    DownloadFile::AppendDataToFile
    //
    //    On error:
    //      DownloadFile::GetDownloadManager
    //      DownloadFile::BytesSoFar
    //      CancelDownload
    //  Process one message in the message loop
    //      DownloadManager::OnDownloadInterrupted
    EXPECT_TRUE(UpdateBuffer(data, length));
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);
    byte_count_[id] += length;

    // Make sure our comparison string is NULL-terminated.
    char* expected_data = new char[length + 1];
    memcpy(expected_data, data, length);
    expected_data[length] = 0;

    EXPECT_CALL(*file, AppendDataToFile(StrEq(expected_data), length))
        .Times(1)
        .WillOnce(Return(error_to_insert));

    if (error_to_insert != net::OK) {
      EXPECT_CALL(*file, BytesSoFar())
          .Times(AtLeast(1))
          .WillRepeatedly(Return(byte_count_[id]));
    }

    download_file_manager_->UpdateDownload(id, download_buffer_.get());

    if (error_to_insert != net::OK) {
      EXPECT_CALL(*download_manager_,
          OnDownloadInterrupted(
              id.local(),
              byte_count_[id],
              "",
              ConvertNetErrorToInterruptReason(error_to_insert,
                                               DOWNLOAD_INTERRUPT_FROM_DISK)));

      ProcessAllPendingMessages();
      ++error_count_[id];
    }

    ClearExpectations(id);
    delete [] expected_data;
  }

  // Renames the download file.
  // |id| is the download ID of the download file.
  // |new_path| is the new file name.
  // |error_to_insert| is the error to inject.  For no error, use net::OK.
  // |is_complete| indicates whether the rename occurs after the download is
  // complete.
  // |replace| indicates whether to replace or uniquify the file.  Only used
  // if |is_complete| true.
  void RenameFile(const DownloadId& id,
                  const FilePath& new_path,
                  net::Error error_to_insert,
                  bool is_complete,
                  bool replace) {
    // Expected call sequence:
    //  RenameInProgressDownloadFile/RenameCompletingDownloadFile
    //    GetDownloadFile
    //    DownloadFile::Rename
    //
    //    On Error:
    //      CancelDownloadOnRename
    //        GetDownloadFile
    //        DownloadFile::GetDownloadManager
    //        No Manager:
    //          DownloadFile::CancelDownloadRequest/return
    //        Has Manager:
    //          DownloadFile::BytesSoFar
    //  Process one message in the message loop
    //          DownloadManager::OnDownloadInterrupted
    //
    //    On Success, if final rename:
    //  Process one message in the message loop
    //    DownloadManager::OnDownloadRenamedToFinalName
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);

    FilePath unique_path = new_path;
    int uniquifier = 0;

    if (is_complete && !replace) {
      uniquifier = content::DownloadFile::GetUniquePathNumber(new_path);
      if (uniquifier > 0)
        content::DownloadFile::AppendNumberToPath(&unique_path, uniquifier);
    }

    EXPECT_CALL(*file, Rename(unique_path))
        .Times(1)
        .WillOnce(Return(error_to_insert));

    if (error_to_insert != net::OK) {
      EXPECT_CALL(*file, GetDownloadManager());

      EXPECT_CALL(*file, BytesSoFar())
          .Times(AtLeast(1))
          .WillRepeatedly(Return(byte_count_[id]));
    }

    if (!is_complete) {
      download_file_manager_->RenameInProgressDownloadFile(id, new_path);
    } else {
      download_file_manager_->RenameCompletingDownloadFile(id,
                                                           new_path,
                                                           replace);
    }

    if (error_to_insert != net::OK) {
      EXPECT_CALL(*download_manager_,
                      OnDownloadInterrupted(
                          id.local(),
                          byte_count_[id],
                          "",
                          ConvertNetErrorToInterruptReason(
                             error_to_insert, DOWNLOAD_INTERRUPT_FROM_DISK)));

      ProcessAllPendingMessages();
      ++error_count_[id];
    } else if (is_complete) {
      EXPECT_CALL(*download_manager_, OnDownloadRenamedToFinalName(
          id.local(), unique_path, uniquifier));
      ProcessAllPendingMessages();
    }

  }

  // Finish the download operation.
  // |id| is the download ID of the download file.
  // |reason| is the interrupt reason to inject.  For no interrupt, use
  // DOWNLOAD_INTERRUPT_REASON_NONE.
  // |security_string| is the extra security information, if interrupted.
  void OnResponseCompleted(const DownloadId& id,
                           InterruptReason reason,
                           const std::string& security_string) {
    //  OnResponseCompleted
    //    GetDownloadFile
    //    DownloadFile::Finish
    //    DownloadFile::GetDownloadManager
    //    DownloadFile::BytesSoFar
    //  Process one message in the message loop
    //
    //    OK:
    //      DownloadFile::GetHash
    //      DownloadManager::OnResponseCompleted
    //
    //    On Error:
    //      DownloadManager::OnDownloadInterrupted
    MockDownloadFile* file = download_file_factory_->GetExistingFile(id);
    ASSERT_TRUE(file != NULL);

    EXPECT_CALL(*file, Finish());
    if (reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
      EXPECT_CALL(*file, GetHash(_))
          .WillOnce(Return(false));
    }
    EXPECT_CALL(*file, BytesSoFar())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(byte_count_[id]));

    download_file_manager_->OnResponseCompleted(id, reason, security_string);

    if (reason == DOWNLOAD_INTERRUPT_REASON_NONE) {
      EXPECT_CALL(*download_manager_,
                  OnResponseCompleted(id.local(), byte_count_[id], ""))
          .Times(1);
    } else {
      EXPECT_EQ(0, error_count_[id]);

      EXPECT_CALL(*download_manager_,
                  OnDownloadInterrupted(id.local(),
                      byte_count_[id],
                      "",
                      reason))
          .Times(1);
    }

    ProcessAllPendingMessages();

    if (reason != DOWNLOAD_INTERRUPT_REASON_NONE)
      ++error_count_[id];

    ClearExpectations(id);
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

    EXPECT_TRUE(NULL == download_file_manager_->GetDownloadFile(id));
  }

 protected:
  scoped_refptr<MockDownloadManager> download_manager_;
  scoped_ptr<MockDownloadRequestHandle> request_handle_;
  MockDownloadFileFactory* download_file_factory_;
  scoped_refptr<DownloadFileManager> download_file_manager_;
  scoped_refptr<content::DownloadBuffer> download_buffer_;

  // Per-download statistics.
  std::map<DownloadId, int64> byte_count_;
  std::map<DownloadId, int> error_count_;

 private:
  MessageLoop loop_;
  scoped_refptr<DownloadIdFactory> id_factory_;

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

TEST_F(DownloadFileManagerTest, CancelAtStart) {
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, CancelBeforeFinished) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, DownloadWithError) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2),
                 net::ERR_FILE_NO_SPACE);
}

TEST_F(DownloadFileManagerTest, CompleteDownload) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, CompleteDownloadWithError) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id,
                      DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED,
                      "");

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameInProgress) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::OK, false, false);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameInProgressWithError) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::ERR_FILE_PATH_TOO_LONG, false, false);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameCompleting) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::OK, false, false);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameCompletingWithError) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::ERR_FILE_PATH_TOO_LONG, false, false);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, RenameTwice) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);
  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);
  FilePath crfoo(FILE_PATH_LITERAL("foo.txt.crdownload"));
  RenameFile(dummy_id, crfoo, net::OK, false, false);
  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::OK, false, false);

  CleanUp(dummy_id);
}

TEST_F(DownloadFileManagerTest, TwoDownloads) {
  // Same as StartDownload, at first.
  DownloadCreateInfo* info = new DownloadCreateInfo;
  DownloadCreateInfo* info2 = new DownloadCreateInfo;
  DownloadId dummy_id(download_manager_.get(), kDummyDownloadId);
  DownloadId dummy_id2(download_manager_.get(), kDummyDownloadId2);

  StartDownload(info, dummy_id);

  UpdateDownload(dummy_id, kTestData1, strlen(kTestData1), net::OK);

  StartDownload(info2, dummy_id2);

  UpdateDownload(dummy_id, kTestData2, strlen(kTestData2), net::OK);

  UpdateDownload(dummy_id2, kTestData1, strlen(kTestData4), net::OK);
  FilePath crbar(FILE_PATH_LITERAL("bar.txt.crdownload"));
  RenameFile(dummy_id2, crbar, net::OK, false, false);

  FilePath crfoo(FILE_PATH_LITERAL("foo.txt.crdownload"));
  RenameFile(dummy_id, crfoo, net::OK, false, false);

  UpdateDownload(dummy_id, kTestData3, strlen(kTestData3), net::OK);

  UpdateDownload(dummy_id2, kTestData2, strlen(kTestData5), net::OK);
  UpdateDownload(dummy_id2, kTestData3, strlen(kTestData6), net::OK);

  OnResponseCompleted(dummy_id2, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  OnResponseCompleted(dummy_id, DOWNLOAD_INTERRUPT_REASON_NONE, "");

  FilePath bar(FILE_PATH_LITERAL("bar.txt"));
  RenameFile(dummy_id2, bar, net::OK, false, false);

  CleanUp(dummy_id2);

  FilePath foo(FILE_PATH_LITERAL("foo.txt"));
  RenameFile(dummy_id, foo, net::OK, false, false);

  CleanUp(dummy_id);
}


// TODO(ahendrickson) -- A test for updating progress.
// Expected call sequence:
//  UpdateInProgressDownloads
//    DownloadFile::GetDownloadFile
//  Process one message in the message loop
//    DownloadManager::UpdateDownload

// TODO(ahendrickson) -- A test for download manager shutdown.
// Expected call sequence:
//  OnDownloadManagerShutdown
//    DownloadFile::GetDownloadManager
//    DownloadFile::CancelDownloadRequest
//    DownloadFile::~DownloadFile
