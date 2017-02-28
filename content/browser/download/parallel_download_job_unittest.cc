// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/mock_download_item_impl.h"
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
      std::unique_ptr<DownloadRequestHandleInterface> request_handle)
      : ParallelDownloadJob(download_item, std::move(request_handle)) {}

  void CreateRequest(int64_t offset, int64_t length) override {
    fake_tasks_.push_back(std::pair<int64_t, int64_t>(offset, length));
  }

  std::vector<std::pair<int64_t, int64_t>> fake_tasks_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJobForTest);
};

class ParallelDownloadJobTest : public testing::Test {
 public:
  void SetUp() override {
    item_delegate_ = base::MakeUnique<DownloadItemImplDelegate>();
    download_item_ =
        base::MakeUnique<NiceMock<MockDownloadItemImpl>>(item_delegate_.get());
    job_ = base::MakeUnique<ParallelDownloadJobForTest>(
        download_item_.get(), base::MakeUnique<MockDownloadRequestHandle>());
  }

  void CreateNewDownloadRequests(int64_t total_bytes,
                                 int64_t bytes_received,
                                 int request_num) {
    job_->request_num_ = request_num;
    job_->ForkRequestsForNewDownload(bytes_received, total_bytes);
  }

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<DownloadItemImplDelegate> item_delegate_;
  std::unique_ptr<MockDownloadItemImpl> download_item_;
  std::unique_ptr<ParallelDownloadJobForTest> job_;
};

// Test if sub tasks are created correctly.
TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequests) {
  EXPECT_TRUE(job_->fake_tasks_.empty());

  // Totally 2 requests for 100 bytes.
  // Original request:  Range:0-49, for 50 bytes.
  // Task 1:  Range:50-99, for 50 bytes.
  CreateNewDownloadRequests(100, 0, 2);
  EXPECT_EQ(1, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(50, job_->fake_tasks_[0].first);
  EXPECT_EQ(50, job_->fake_tasks_[0].second);
  job_->fake_tasks_.clear();

  // Totally 3 requests for 100 bytes.
  // Original request:  Range:0-32, for 33 bytes.
  // Task 1:  Range:33-65, for 33 bytes.
  // Task 2:  Range:66-99, for 34 bytes.
  CreateNewDownloadRequests(100, 0, 3);
  EXPECT_EQ(2, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(33, job_->fake_tasks_[0].first);
  EXPECT_EQ(33, job_->fake_tasks_[0].second);
  EXPECT_EQ(66, job_->fake_tasks_[1].first);
  EXPECT_EQ(34, job_->fake_tasks_[1].second);
  job_->fake_tasks_.clear();

  // Totally 3 requests for 100 bytes. Start from the 17th byte.
  // Original request:  Range:17-43, for 27 bytes.
  // Task 1:  Range:44-70, for 27 bytes.
  // Task 2:  Range:71-99, for 29 bytes.
  CreateNewDownloadRequests(100, 17, 3);
  EXPECT_EQ(2, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(44, job_->fake_tasks_[0].first);
  EXPECT_EQ(27, job_->fake_tasks_[0].second);
  EXPECT_EQ(71, job_->fake_tasks_[1].first);
  EXPECT_EQ(29, job_->fake_tasks_[1].second);
  job_->fake_tasks_.clear();

  // Less than 2 requests, do nothing.
  CreateNewDownloadRequests(100, 17, 1);
  EXPECT_TRUE(job_->fake_tasks_.empty());
  CreateNewDownloadRequests(100, 17, 0);
  EXPECT_TRUE(job_->fake_tasks_.empty());

  // Received bytes are no less than total bytes, do nothing.
  CreateNewDownloadRequests(100, 100, 3);
  EXPECT_TRUE(job_->fake_tasks_.empty());
  CreateNewDownloadRequests(100, 255, 3);
  EXPECT_TRUE(job_->fake_tasks_.empty());

  // Edge cases for 0 bytes.
  CreateNewDownloadRequests(0, 0, 3);
  EXPECT_TRUE(job_->fake_tasks_.empty());

  // 2 bytes left for 3 additional requests. Only 1 are built.
  // Original request:  Range:98-98, for 1 byte.
  // Task 1:  Range:99-99, for 1 byte.
  CreateNewDownloadRequests(100, 98, 4);
  EXPECT_EQ(1, static_cast<int>(job_->fake_tasks_.size()));
  EXPECT_EQ(99, job_->fake_tasks_[0].first);
  EXPECT_EQ(1, job_->fake_tasks_[0].second);
  job_->fake_tasks_.clear();
}

}  // namespace content
