// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_job.h"
#include "content/browser/download/mock_download_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockDownloadJobManager : public DownloadJob::Manager {
 public:
  MockDownloadJobManager() = default;
  ~MockDownloadJobManager() = default;

  MOCK_METHOD1(OnSavingStarted, void(DownloadJob* download_job));
  MOCK_METHOD1(SetDangerType, void(DownloadDangerType));
  MOCK_METHOD2(OnDownloadInterrupted,
               void(DownloadJob* download_job, DownloadInterruptReason reason));
  MOCK_METHOD1(OnDownloadComplete, void(DownloadJob* download_job));
};

// Test for DownloadJob base class functionalities.
class DownloadJobTest : public testing::Test {
 public:
  DownloadJobTest() = default;
  ~DownloadJobTest() override = default;

  void SetUp() override {
    download_job_manager_ = base::MakeUnique<MockDownloadJobManager>();
    EXPECT_TRUE(download_job_manager_.get());
    download_job_ = base::MakeUnique<MockDownloadJob>();
  }

  std::unique_ptr<MockDownloadJobManager> download_job_manager_;
  std::unique_ptr<MockDownloadJob> download_job_;
};

TEST_F(DownloadJobTest, AttachAndDetach) {
  // Ensure the manager should be valid only between attach and detach call.
  EXPECT_FALSE(download_job_->manager());
  download_job_->OnAttached(download_job_manager_.get());
  EXPECT_TRUE(download_job_->manager());
  download_job_->OnBeforeDetach();
  EXPECT_FALSE(download_job_->manager());
}

}  // namespace content
