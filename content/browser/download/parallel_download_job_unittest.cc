// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/mock_download_item_impl.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace content {

namespace {

class MockDownloadRequestHandle : public DownloadRequestHandleInterface {
 public:
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(PauseRequest, void());
  MOCK_CONST_METHOD0(ResumeRequest, void());
  MOCK_CONST_METHOD0(CancelRequest, void());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

}  // namespace

class ParallelDownloadJobForTest : public ParallelDownloadJob {
 public:
  ParallelDownloadJobForTest(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadCreateInfo& create_info,
      int request_count)
      : ParallelDownloadJob(download_item,
                            std::move(request_handle),
                            create_info),
        request_count_(request_count) {}

  void CreateRequest(int64_t offset, int64_t length) override {
    fake_tasks_.push_back(std::pair<int64_t, int64_t>(offset, length));
  }

  int GetParallelRequestCount() const override { return request_count_; }

  std::vector<std::pair<int64_t, int64_t>> fake_tasks_;

 private:
  int request_count_;
  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJobForTest);
};

class ParallelDownloadJobTest : public testing::Test {
 public:
  void CreateParallelJob(int64_t offset,
                         int64_t content_length,
                         const DownloadItem::ReceivedSlices& slices,
                         int request_count) {
    item_delegate_ = base::MakeUnique<DownloadItemImplDelegate>();
    download_item_ = base::MakeUnique<NiceMock<MockDownloadItemImpl>>(
        item_delegate_.get(), slices);
    DownloadCreateInfo info;
    info.offset = offset;
    info.total_bytes = content_length;
    job_ = base::MakeUnique<ParallelDownloadJobForTest>(
        download_item_.get(), base::MakeUnique<MockDownloadRequestHandle>(),
        info, request_count);
  }

  void DestroyParallelJob() {
    job_.reset();
    download_item_.reset();
    item_delegate_.reset();
  }

  void BuildParallelRequests() { job_->BuildParallelRequests(); }

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<DownloadItemImplDelegate> item_delegate_;
  std::unique_ptr<MockDownloadItemImpl> download_item_;
  std::unique_ptr<ParallelDownloadJobForTest> job_;
};

// Test if parallel requests can be built correctly for a new download.
TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequests) {
  // Totally 2 requests for 100 bytes.
  // Original request:  Range:0-49, for 50 bytes.
  // Task 1:  Range:50-, for 50 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(50, job_->fake_tasks_[0].first);
  EXPECT_EQ(0, job_->fake_tasks_[0].second);
  job_->fake_tasks_.clear();
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes.
  // Original request:  Range:0-32, for 33 bytes.
  // Task 1:  Range:33-65, for 33 bytes.
  // Task 2:  Range:66-, for 34 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(33, job_->fake_tasks_[0].first);
  EXPECT_EQ(33, job_->fake_tasks_[0].second);
  EXPECT_EQ(66, job_->fake_tasks_[1].first);
  EXPECT_EQ(0, job_->fake_tasks_[1].second);
  job_->fake_tasks_.clear();
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes. Start from the 17th byte.
  // Original request:  Range:17-43, for 27 bytes.
  // Task 1:  Range:44-70, for 27 bytes.
  // Task 2:  Range:71-99, for 29 bytes.
  CreateParallelJob(17, 83, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(44, job_->fake_tasks_[0].first);
  EXPECT_EQ(27, job_->fake_tasks_[0].second);
  EXPECT_EQ(71, job_->fake_tasks_[1].first);
  EXPECT_EQ(0, job_->fake_tasks_[1].second);
  job_->fake_tasks_.clear();
  DestroyParallelJob();

  // Less than 2 requests, do nothing.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 1);
  BuildParallelRequests();
  EXPECT_TRUE(job_->fake_tasks_.empty());
  DestroyParallelJob();

  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 0);
  BuildParallelRequests();
  EXPECT_TRUE(job_->fake_tasks_.empty());
  DestroyParallelJob();

  // Content-length is 0, do nothing.
  CreateParallelJob(100, 0, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_TRUE(job_->fake_tasks_.empty());
  DestroyParallelJob();

  CreateParallelJob(0, 0, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_TRUE(job_->fake_tasks_.empty());
  DestroyParallelJob();

  // 2 bytes left for 3 additional requests. Only 1 are built.
  // Original request:  Range:98-98, for 1 byte.
  // Task 1:  Range:99-, for 1 byte.
  CreateParallelJob(98, 2, DownloadItem::ReceivedSlices(), 4);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(99, job_->fake_tasks_[0].first);
  EXPECT_EQ(0, job_->fake_tasks_[0].second);
  job_->fake_tasks_.clear();
  DestroyParallelJob();
}

}  // namespace content
