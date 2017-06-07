// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/download/download_destination_observer.h"
#include "content/browser/download/download_file_impl.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/mock_download_item_impl.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace content {

namespace {

class MockDownloadRequestHandle : public DownloadRequestHandleInterface {
 public:
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(PauseRequest, void());
  MOCK_CONST_METHOD0(ResumeRequest, void());
  MOCK_CONST_METHOD1(CancelRequest, void(bool));
  MOCK_CONST_METHOD0(DebugString, std::string());
};

class MockDownloadDestinationObserver : public DownloadDestinationObserver {
 public:
  MOCK_METHOD3(DestinationUpdate,
               void(int64_t,
                    int64_t,
                    const std::vector<DownloadItem::ReceivedSlice>&));
  void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) override {}
  void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) override {}
  MOCK_METHOD2(CurrentUpdateStatus, void(int64_t, int64_t));
};

class MockByteStreamReader : public ByteStreamReader {
 public:
  MOCK_METHOD2(Read,
               ByteStreamReader::StreamState(scoped_refptr<net::IOBuffer>*,
                                             size_t*));
  MOCK_CONST_METHOD0(GetStatus, int());
  MOCK_METHOD1(RegisterCallback, void(const base::Closure&));
};

}  // namespace

class ParallelDownloadJobForTest : public ParallelDownloadJob {
 public:
  ParallelDownloadJobForTest(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadCreateInfo& create_info,
      int request_count,
      int64_t min_slice_size,
      int min_remaining_time)
      : ParallelDownloadJob(download_item,
                            std::move(request_handle),
                            create_info),
        request_count_(request_count),
        min_slice_size_(min_slice_size),
        min_remaining_time_(min_remaining_time) {}

  void CreateRequest(int64_t offset, int64_t length) override {
    std::unique_ptr<DownloadWorker> worker =
        base::MakeUnique<DownloadWorker>(this, offset, length);

    DCHECK(workers_.find(offset) == workers_.end());
    workers_[offset] = std::move(worker);
  }

  ParallelDownloadJob::WorkerMap& workers() { return workers_; }

  int GetParallelRequestCount() const override { return request_count_; }
  int64_t GetMinSliceSize() const override { return min_slice_size_; }
  int GetMinRemainingTimeInSeconds() const override {
    return min_remaining_time_;
  }

  void OnByteStreamReady(
      DownloadWorker* worker,
      std::unique_ptr<ByteStreamReader> stream_reader) override {
    CountOnByteStreamReady();
  }

  MOCK_METHOD0(CountOnByteStreamReady, void());

 private:
  int request_count_;
  int min_slice_size_;
  int min_remaining_time_;
  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJobForTest);
};

class ParallelDownloadJobTest : public testing::Test {
 public:
  void CreateParallelJob(int64_t initial_request_offset,
                         int64_t content_length,
                         const DownloadItem::ReceivedSlices& slices,
                         int request_count,
                         int64_t min_slice_size,
                         int min_remaining_time) {
    item_delegate_ = base::MakeUnique<DownloadItemImplDelegate>();
    download_item_ = base::MakeUnique<NiceMock<MockDownloadItemImpl>>(
        item_delegate_.get(), slices);
    EXPECT_CALL(*download_item_, GetTotalBytes())
        .WillRepeatedly(
            testing::Return(initial_request_offset + content_length));
    EXPECT_CALL(*download_item_, GetReceivedBytes())
        .WillRepeatedly(testing::Return(initial_request_offset));

    DownloadCreateInfo info;
    info.offset = initial_request_offset;
    info.total_bytes = content_length;
    std::unique_ptr<MockDownloadRequestHandle> request_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    mock_request_handle_ = request_handle.get();
    job_ = base::MakeUnique<ParallelDownloadJobForTest>(
        download_item_.get(), std::move(request_handle), info, request_count,
        min_slice_size, min_remaining_time);
    file_initialized_ = false;
  }

  void DestroyParallelJob() {
    job_.reset();
    download_item_.reset();
    item_delegate_.reset();
    mock_request_handle_ = nullptr;
  }

  void BuildParallelRequests() { job_->BuildParallelRequests(); }

  bool IsJobCanceled() const { return job_->is_canceled_; };

  void MakeWorkerReady(
      DownloadWorker* worker,
      std::unique_ptr<MockDownloadRequestHandle> request_handle) {
    UrlDownloader::Delegate* delegate =
        static_cast<UrlDownloader::Delegate*>(worker);
    std::unique_ptr<DownloadCreateInfo> create_info =
        base::MakeUnique<DownloadCreateInfo>();
    create_info->request_handle = std::move(request_handle);
    delegate->OnUrlDownloaderStarted(
        std::move(create_info), std::unique_ptr<ByteStreamReader>(),
        DownloadUrlParameters::OnStartedCallback());
  }

  void VerifyWorker(int64_t offset, int64_t length) const {
    EXPECT_TRUE(job_->workers_.find(offset) != job_->workers_.end());
    EXPECT_EQ(offset, job_->workers_[offset]->offset());
    EXPECT_EQ(length, job_->workers_[offset]->length());
  }

  void OnFileInitialized(DownloadInterruptReason result) {
    file_initialized_ = true;
  }

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<DownloadItemImplDelegate> item_delegate_;
  std::unique_ptr<MockDownloadItemImpl> download_item_;
  std::unique_ptr<ParallelDownloadJobForTest> job_;
  bool file_initialized_;
  // Request handle for the original request.
  MockDownloadRequestHandle* mock_request_handle_;
};

// Test if parallel requests can be built correctly for a new download without
// existing slices.
TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequestsWithoutSlices) {
  // Totally 2 requests for 100 bytes.
  // Original request:  Range:0-49, for 50 bytes.
  // Task 1:  Range:50-, for 50 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->workers().size()));
  VerifyWorker(50, 0);
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes.
  // Original request:  Range:0-32, for 33 bytes.
  // Task 1:  Range:33-65, for 33 bytes.
  // Task 2:  Range:66-, for 34 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->workers().size()));
  VerifyWorker(33, 33);
  VerifyWorker(66, 0);
  DestroyParallelJob();

  // Less than 2 requests, do nothing.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 1, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 0, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // Content-length is 0, do nothing.
  CreateParallelJob(0, 0, DownloadItem::ReceivedSlices(), 3, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();
}

TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequestsWithSlices) {
  // File size: 100 bytes.
  // Received slices: [0, 17]
  // Original request: Range:12-. Content-length: 88.
  // Totally 3 requests for 83 bytes.
  // Original request:  Range:12-43.
  // Task 1:  Range:44-70, for 27 bytes.
  // Task 2:  Range:71-, for 29 bytes.
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 17)};
  CreateParallelJob(12, 88, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->workers().size()));
  VerifyWorker(44, 27);
  VerifyWorker(71, 0);
  DestroyParallelJob();

  // File size: 100 bytes.
  // Received slices: [0, 60], Range:0-59.
  // Original request: Range:60-. Content-length: 40.
  // 40 bytes left for 4 requests. Only 1 additional request.
  // Original request: Range:60-79, for 20 bytes.
  // Task 1:  Range:80-, for 20 bytes.
  slices = {DownloadItem::ReceivedSlice(0, 60)};
  CreateParallelJob(60, 40, slices, 4, 20, 10);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->workers().size()));
  VerifyWorker(80, 0);
  DestroyParallelJob();

  // Content-Length is 0, no additional requests.
  slices = {DownloadItem::ReceivedSlice(0, 100)};
  CreateParallelJob(100, 0, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // File size: 100 bytes.
  // Original request: Range:0-. Content-length: 12(Incorrect server header).
  // The request count is 2, however the file contains 3 holes, and we don't
  // know if the last slice is completed, so there should be 3 requests in
  // parallel and the last request is an out-of-range request.
  slices = {
      DownloadItem::ReceivedSlice(10, 10), DownloadItem::ReceivedSlice(20, 10),
      DownloadItem::ReceivedSlice(40, 10), DownloadItem::ReceivedSlice(90, 10)};
  CreateParallelJob(0, 12, slices, 2, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(3, static_cast<int>(job_->workers().size()));
  VerifyWorker(30, 10);
  VerifyWorker(50, 40);
  VerifyWorker(100, 0);
  DestroyParallelJob();
}

// Pause, cancel, resume can be called before or after the worker establish
// the byte stream.
// These tests ensure the states consistency between the job and workers.

// Ensure cancel before building the requests will result in no requests are
// built.
TEST_F(ParallelDownloadJobTest, EarlyCancelBeforeBuildRequests) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);
  EXPECT_CALL(*mock_request_handle_, CancelRequest(_));

  // Job is canceled before building parallel requests.
  job_->Cancel(true);
  EXPECT_TRUE(IsJobCanceled());

  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());

  DestroyParallelJob();
}

// Ensure cancel before adding the byte stream will result in workers being
// canceled.
TEST_F(ParallelDownloadJobTest, EarlyCancelBeforeByteStreamReady) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);
  EXPECT_CALL(*mock_request_handle_, CancelRequest(_));

  BuildParallelRequests();
  VerifyWorker(50, 0);

  // Job is canceled after building parallel requests and before byte streams
  // are added to the file sink.
  job_->Cancel(true);
  EXPECT_TRUE(IsJobCanceled());

  for (auto& worker : job_->workers()) {
    std::unique_ptr<MockDownloadRequestHandle> mock_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    EXPECT_CALL(*mock_handle.get(), CancelRequest(_));
    MakeWorkerReady(worker.second.get(), std::move(mock_handle));
  }

  DestroyParallelJob();
}

// Ensure pause before adding the byte stream will result in workers being
// paused.
TEST_F(ParallelDownloadJobTest, EarlyPauseBeforeByteStreamReady) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);
  EXPECT_CALL(*mock_request_handle_, PauseRequest());

  BuildParallelRequests();
  VerifyWorker(50, 0);

  // Job is paused after building parallel requests and before adding the byte
  // stream to the file sink.
  job_->Pause();
  EXPECT_TRUE(job_->is_paused());

  for (auto& worker : job_->workers()) {
    EXPECT_CALL(*job_.get(), CountOnByteStreamReady());
    std::unique_ptr<MockDownloadRequestHandle> mock_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    EXPECT_CALL(*mock_handle.get(), PauseRequest());
    MakeWorkerReady(worker.second.get(), std::move(mock_handle));
  }

  DestroyParallelJob();
}

// Test that parallel request is not created if the remaining content can be
// finish downloading soon.
TEST_F(ParallelDownloadJobTest, RemainingContentWillFinishSoon) {
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 99)};
  CreateParallelJob(99, 1, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(0, static_cast<int>(job_->workers().size()));

  DestroyParallelJob();
}

// Test that parallel request is not created until download file is initialized.
TEST_F(ParallelDownloadJobTest, ParallelRequestNotCreatedUntilFileInitialized) {
  auto save_info = base::MakeUnique<DownloadSaveInfo>();
  StrictMock<MockByteStreamReader>* input_stream =
      new StrictMock<MockByteStreamReader>();
  auto observer =
      base::MakeUnique<StrictMock<MockDownloadDestinationObserver>>();
  base::WeakPtrFactory<DownloadDestinationObserver> observer_factory(
      observer.get());
  auto download_file = base::MakeUnique<DownloadFileImpl>(
      std::move(save_info), base::FilePath(),
      std::unique_ptr<ByteStreamReader>(input_stream), net::NetLogWithSource(),
      observer_factory.GetWeakPtr());
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 0, 0);
  job_->Start(download_file.get(),
              base::Bind(&ParallelDownloadJobTest::OnFileInitialized,
                         base::Unretained(this)),
              DownloadItem::ReceivedSlices());
  EXPECT_FALSE(file_initialized_);
  EXPECT_EQ(0u, job_->workers().size());
  EXPECT_CALL(*input_stream, RegisterCallback(_));
  EXPECT_CALL(*input_stream, Read(_, _));
  EXPECT_CALL(*(observer.get()), DestinationUpdate(_, _, _));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(file_initialized_);
  EXPECT_EQ(1u, job_->workers().size());
  DestroyParallelJob();
}

}  // namespace content
