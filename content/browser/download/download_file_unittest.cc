// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_request_handle.h"
#include "content/browser/power_save_blocker.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/mock_download_manager.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;
using content::DownloadFile;
using content::DownloadId;
using content::DownloadManager;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrictMock;

namespace {

class MockByteStreamReader : public content::ByteStreamReader {
 public:
  MockByteStreamReader() {}
  ~MockByteStreamReader() {}

  // ByteStream functions
  MOCK_METHOD2(Read, content::ByteStreamReader::StreamState(
      scoped_refptr<net::IOBuffer>*, size_t*));
  MOCK_CONST_METHOD0(GetStatus, content::DownloadInterruptReason());
  MOCK_METHOD1(RegisterCallback, void(const base::Closure&));
};

}  // namespace

DownloadId::Domain kValidIdDomain = "valid DownloadId::Domain";

class DownloadFileTest : public testing::Test {
 public:

  static const char* kTestData1;
  static const char* kTestData2;
  static const char* kTestData3;
  static const char* kDataHash;
  static const int32 kDummyDownloadId;
  static const int kDummyChildId;
  static const int kDummyRequestId;

  // We need a UI |BrowserThread| in order to destruct |download_manager_|,
  // which has trait |BrowserThread::DeleteOnUIThread|.  Without this,
  // calling Release() on |download_manager_| won't ever result in its
  // destructor being called and we get a leak.
  DownloadFileTest() :
      ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE, &loop_) {
  }

  ~DownloadFileTest() {
  }

  void SetUpdateDownloadInfo(int32 id, int64 bytes, int64 bytes_per_sec,
                             const std::string& hash_state) {
    bytes_ = bytes;
    bytes_per_sec_ = bytes_per_sec;
    hash_state_ = hash_state;
  }

  virtual void SetUp() {
    download_manager_ = new StrictMock<content::MockDownloadManager>;
    EXPECT_CALL(*(download_manager_.get()),
                UpdateDownload(
                    DownloadId(kValidIdDomain, kDummyDownloadId + 0).local(),
                    _, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(this, &DownloadFileTest::SetUpdateDownloadInfo));
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

  // Mock calls to this function are forwarded here.
  void RegisterCallback(base::Closure sink_callback) {
    sink_callback_ = sink_callback;
  }

  virtual bool CreateDownloadFile(int offset, bool calculate_hash) {
    // There can be only one.
    DCHECK(!download_file_.get());

    input_stream_ = new StrictMock<MockByteStreamReader>();

    // TODO: Need to actually create a function that'll set the variables
    // based on the inputs from the callback.
    EXPECT_CALL(*input_stream_, RegisterCallback(_))
        .WillOnce(Invoke(this, &DownloadFileTest::RegisterCallback))
        .RetiresOnSaturation();

    DownloadCreateInfo info;
    info.download_id = DownloadId(kValidIdDomain, kDummyDownloadId + offset);
    // info.request_handle default constructed to null.
    info.save_info.file_stream = file_stream_;
    download_file_.reset(
        new DownloadFileImpl(
            &info,
            scoped_ptr<content::ByteStreamReader>(input_stream_).Pass(),
            new DownloadRequestHandle(),
            download_manager_, calculate_hash,
            scoped_ptr<content::PowerSaveBlocker>(NULL).Pass(),
            net::BoundNetLog()));

    EXPECT_CALL(*input_stream_, Read(_, _))
        .WillOnce(Return(content::ByteStreamReader::STREAM_EMPTY))
        .RetiresOnSaturation();
    content::DownloadInterruptReason result = download_file_->Initialize();
    ::testing::Mock::VerifyAndClearExpectations(input_stream_);
    return result == content::DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  virtual void DestroyDownloadFile(int offset) {
    EXPECT_EQ(kDummyDownloadId + offset, download_file_->Id());
    EXPECT_EQ(download_manager_, download_file_->GetDownloadManager());
    EXPECT_FALSE(download_file_->InProgress());
    EXPECT_EQ(static_cast<int64>(expected_data_.size()),
              download_file_->BytesSoFar());

    // Make sure the data has been properly written to disk.
    std::string disk_data;
    EXPECT_TRUE(file_util::ReadFileToString(download_file_->FullPath(),
                                            &disk_data));
    EXPECT_EQ(expected_data_, disk_data);

    // Make sure the Browser and File threads outlive the DownloadFile
    // to satisfy thread checks inside it.
    download_file_.reset();
  }

  // Setup the stream to do be a data append; don't actually trigger
  // the callback or do verifications.
  void SetupDataAppend(const char **data_chunks, size_t num_chunks,
                       ::testing::Sequence s) {
    DCHECK(input_stream_);
    for (size_t i = 0; i < num_chunks; i++) {
      const char *source_data = data_chunks[i];
      size_t length = strlen(source_data);
      scoped_refptr<net::IOBuffer> data = new net::IOBuffer(length);
      memcpy(data->data(), source_data, length);
      EXPECT_CALL(*input_stream_, Read(_, _))
          .InSequence(s)
          .WillOnce(DoAll(SetArgPointee<0>(data),
                          SetArgPointee<1>(length),
                          Return(content::ByteStreamReader::STREAM_HAS_DATA)))
          .RetiresOnSaturation();
      expected_data_ += source_data;
    }
  }

  void VerifyStreamAndSize() {
    ::testing::Mock::VerifyAndClearExpectations(input_stream_);
    int64 size;
    EXPECT_TRUE(file_util::GetFileSize(download_file_->FullPath(), &size));
    EXPECT_EQ(expected_data_.size(), static_cast<size_t>(size));
  }

  // TODO(rdsmith): Manage full percentage issues properly.
  void AppendDataToFile(const char **data_chunks, size_t num_chunks) {
    ::testing::Sequence s1;
    SetupDataAppend(data_chunks, num_chunks, s1);
    EXPECT_CALL(*input_stream_, Read(_, _))
        .InSequence(s1)
        .WillOnce(Return(content::ByteStreamReader::STREAM_EMPTY))
        .RetiresOnSaturation();
    sink_callback_.Run();
    VerifyStreamAndSize();
  }

  void SetupFinishStream(content::DownloadInterruptReason interrupt_reason,
                       ::testing::Sequence s) {
    EXPECT_CALL(*input_stream_, Read(_, _))
        .InSequence(s)
        .WillOnce(Return(content::ByteStreamReader::STREAM_COMPLETE))
        .RetiresOnSaturation();
    EXPECT_CALL(*input_stream_, GetStatus())
        .InSequence(s)
        .WillOnce(Return(interrupt_reason))
        .RetiresOnSaturation();
    EXPECT_CALL(*input_stream_, RegisterCallback(_))
        .RetiresOnSaturation();
  }

  void FinishStream(content::DownloadInterruptReason interrupt_reason,
                  bool check_download_manager) {
    ::testing::Sequence s1;
    SetupFinishStream(interrupt_reason, s1);
    sink_callback_.Run();
    VerifyStreamAndSize();
    if (check_download_manager) {
      EXPECT_CALL(*download_manager_, OnResponseCompleted(_, _, _));
      loop_.RunAllPending();
      ::testing::Mock::VerifyAndClearExpectations(download_manager_.get());
      EXPECT_CALL(*(download_manager_.get()),
                  UpdateDownload(
                      DownloadId(kValidIdDomain, kDummyDownloadId + 0).local(),
                      _, _, _))
          .Times(AnyNumber())
          .WillRepeatedly(Invoke(this,
                                 &DownloadFileTest::SetUpdateDownloadInfo));
    }
  }

 protected:
  scoped_refptr<StrictMock<content::MockDownloadManager> > download_manager_;

  linked_ptr<net::FileStream> file_stream_;

  // DownloadFile instance we are testing.
  scoped_ptr<DownloadFile> download_file_;

  // Stream for sending data into the download file.
  // Owned by download_file_; will be alive for lifetime of download_file_.
  StrictMock<MockByteStreamReader>* input_stream_;

  // Sink callback data for stream.
  base::Closure sink_callback_;

  // Latest update sent to the download manager.
  int64 bytes_;
  int64 bytes_per_sec_;
  std::string hash_state_;

  MessageLoop loop_;

 private:
  // UI thread.
  BrowserThreadImpl ui_thread_;
  // File thread to satisfy debug checks in DownloadFile.
  BrowserThreadImpl file_thread_;

  // Keep track of what data should be saved to the disk file.
  std::string expected_data_;
};

const char* DownloadFileTest::kTestData1 =
    "Let's write some data to the file!\n";
const char* DownloadFileTest::kTestData2 = "Writing more data.\n";
const char* DownloadFileTest::kTestData3 = "Final line.";
const char* DownloadFileTest::kDataHash =
    "CBF68BF10F8003DB86B31343AFAC8C7175BD03FB5FC905650F8C80AF087443A8";

const int32 DownloadFileTest::kDummyDownloadId = 23;
const int DownloadFileTest::kDummyChildId = 3;
const int DownloadFileTest::kDummyRequestId = 67;

// Rename the file before any data is downloaded, after some has, after it all
// has, and after it's closed.
TEST_F(DownloadFileTest, RenameFileFinal) {
  ASSERT_TRUE(CreateDownloadFile(0, true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));
  FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));
  FilePath path_2(initial_path.InsertBeforeExtensionASCII("_2"));
  FilePath path_3(initial_path.InsertBeforeExtensionASCII("_3"));
  FilePath path_4(initial_path.InsertBeforeExtensionASCII("_4"));

  // Rename the file before downloading any data.
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
            download_file_->Rename(path_1));
  FilePath renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_1, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(initial_path));
  EXPECT_TRUE(file_util::PathExists(path_1));

  // Download the data.
  const char* chunks1[] = { kTestData1, kTestData2 };
  AppendDataToFile(chunks1, 2);

  // Rename the file after downloading some data.
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
            download_file_->Rename(path_2));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_2, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_1));
  EXPECT_TRUE(file_util::PathExists(path_2));

  const char* chunks2[] = { kTestData3 };
  AppendDataToFile(chunks2, 1);

  // Rename the file after downloading all the data.
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
            download_file_->Rename(path_3));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_3, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_2));
  EXPECT_TRUE(file_util::PathExists(path_3));

  // Should not be able to get the hash until the file is closed.
  std::string hash;
  EXPECT_FALSE(download_file_->GetHash(&hash));
  FinishStream(content::DOWNLOAD_INTERRUPT_REASON_NONE, true);
  loop_.RunAllPending();

  // Rename the file after downloading all the data and closing the file.
  EXPECT_EQ(content::DOWNLOAD_INTERRUPT_REASON_NONE,
            download_file_->Rename(path_4));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_4, renamed_path);

  // Check the files.
  EXPECT_FALSE(file_util::PathExists(path_3));
  EXPECT_TRUE(file_util::PathExists(path_4));

  // Check the hash.
  EXPECT_TRUE(download_file_->GetHash(&hash));
  EXPECT_EQ(kDataHash, base::HexEncode(hash.data(), hash.size()));

  DestroyDownloadFile(0);
}

// Various tests of the StreamActive method.
TEST_F(DownloadFileTest, StreamEmptySuccess) {
  ASSERT_TRUE(CreateDownloadFile(0, true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));

  // Test that calling the sink_callback_ on an empty stream shouldn't
  // do anything.
  AppendDataToFile(NULL, 0);
  ::testing::Mock::VerifyAndClearExpectations(download_manager_.get());
  EXPECT_CALL(*(download_manager_.get()),
              UpdateDownload(
                  DownloadId(kValidIdDomain, kDummyDownloadId + 0).local(),
                  _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(this, &DownloadFileTest::SetUpdateDownloadInfo));

  // Finish the download this way and make sure we see it on the
  // DownloadManager.
  EXPECT_CALL(*(download_manager_.get()),
              OnResponseCompleted(DownloadId(kValidIdDomain,
                                             kDummyDownloadId + 0).local(),
                                  0, _));
  FinishStream(content::DOWNLOAD_INTERRUPT_REASON_NONE, false);

  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamEmptyError) {
  ASSERT_TRUE(CreateDownloadFile(0, true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));

  // Finish the download in error and make sure we see it on the
  // DownloadManager.
  EXPECT_CALL(*(download_manager_.get()),
              OnDownloadInterrupted(
                  DownloadId(kValidIdDomain, kDummyDownloadId + 0).local(),
                  0, _,
                  content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED));
  FinishStream(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED, false);

  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamNonEmptySuccess) {
  ASSERT_TRUE(CreateDownloadFile(0, true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));

  const char* chunks1[] = { kTestData1, kTestData2 };
  ::testing::Sequence s1;
  SetupDataAppend(chunks1, 2, s1);
  SetupFinishStream(content::DOWNLOAD_INTERRUPT_REASON_NONE, s1);
  EXPECT_CALL(*(download_manager_.get()),
              OnResponseCompleted(DownloadId(kValidIdDomain,
                                             kDummyDownloadId + 0).local(),
                                  strlen(kTestData1) + strlen(kTestData2),
                                  _));
  sink_callback_.Run();
  VerifyStreamAndSize();
  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamNonEmptyError) {
  ASSERT_TRUE(CreateDownloadFile(0, true));
  FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(file_util::PathExists(initial_path));

  const char* chunks1[] = { kTestData1, kTestData2 };
  ::testing::Sequence s1;
  SetupDataAppend(chunks1, 2, s1);
  SetupFinishStream(content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
                    s1);
  EXPECT_CALL(*(download_manager_.get()),
              OnDownloadInterrupted(
                  DownloadId(kValidIdDomain, kDummyDownloadId + 0).local(),
                  strlen(kTestData1) + strlen(kTestData2), _,
                  content::DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED));
  sink_callback_.Run();
  VerifyStreamAndSize();
  DestroyDownloadFile(0);
}

// Send some data, wait 3/4s of a second, run the message loop, and
// confirm the values the DownloadManager received are correct.
TEST_F(DownloadFileTest, ConfirmUpdate) {
  CreateDownloadFile(0, true);

  const char* chunks1[] = { kTestData1, kTestData2 };
  AppendDataToFile(chunks1, 2);

  // Run the message loops for 750ms and check for results.
  loop_.PostDelayedTask(FROM_HERE, MessageLoop::QuitClosure(),
                         base::TimeDelta::FromMilliseconds(750));
  loop_.Run();

  EXPECT_EQ(static_cast<int64>(strlen(kTestData1) + strlen(kTestData2)),
            bytes_);
  EXPECT_EQ(download_file_->GetHashState(), hash_state_);

  FinishStream(content::DOWNLOAD_INTERRUPT_REASON_NONE, true);
  DestroyDownloadFile(0);
}
