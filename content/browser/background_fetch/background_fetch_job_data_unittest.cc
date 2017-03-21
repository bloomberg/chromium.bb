// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_job_data.h"

#include "base/memory/ptr_util.h"
#include "content/browser/background_fetch/background_fetch_job_info.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const char kResource[] = "https://example.com/resource.html";
const char kTag[] = "TestRequestTag";

}  // namespace

class BackgroundFetchJobDataTest : public testing::Test {
 protected:
  std::unique_ptr<BackgroundFetchJobData> CreateMinimalJobData() {
    DCHECK(request_infos_.empty());
    // Create a JobData with a single entry.
    request_infos_.emplace_back(GURL(kResource), kTag);
    return base::MakeUnique<BackgroundFetchJobData>(request_infos_);
  }

  std::unique_ptr<BackgroundFetchJobData> CreateSmallJobData() {
    DCHECK(request_infos_.empty());
    // Create 10 BackgroundFetchRequestInfos.
    for (int i = 0; i < 10; i++) {
      request_infos_.emplace_back(GURL(kResource), kTag);
    }
    return base::MakeUnique<BackgroundFetchJobData>(request_infos_);
  }

  const BackgroundFetchRequestInfos& request_infos() const {
    return request_infos_;
  }

 private:
  BackgroundFetchRequestInfos request_infos_;
};

TEST_F(BackgroundFetchJobDataTest, CompleteJob) {
  std::unique_ptr<BackgroundFetchJobData> job_data = CreateSmallJobData();
  const BackgroundFetchRequestInfos& request_infos =
      BackgroundFetchJobDataTest::request_infos();
  ASSERT_EQ(10U, request_infos.size());

  // Get all of the fetch requests from the BackgroundFetchJobData.
  for (int i = 0; i < 10; i++) {
    EXPECT_FALSE(job_data->IsComplete());
    ASSERT_TRUE(job_data->HasRequestsRemaining());
    const BackgroundFetchRequestInfo& request_info =
        job_data->GetNextBackgroundFetchRequestInfo();
    EXPECT_EQ(request_info.tag(), kTag);
    EXPECT_EQ(request_info.state(),
              DownloadItem::DownloadState::MAX_DOWNLOAD_STATE);
    EXPECT_EQ(request_info.interrupt_reason(),
              DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE);
  }

  // At this point, all the fetches have been started, but none finished.
  EXPECT_FALSE(job_data->HasRequestsRemaining());

  // Complete all of the fetch requests.
  for (int i = 0; i < 10; i++) {
    EXPECT_FALSE(job_data->IsComplete());
    EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
        request_infos[i].guid(), DownloadItem::DownloadState::COMPLETE,
        DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  }

  // All requests are complete now.
  EXPECT_TRUE(job_data->IsComplete());
}

TEST_F(BackgroundFetchJobDataTest, OutOfOrderCompletion) {
  std::unique_ptr<BackgroundFetchJobData> job_data = CreateSmallJobData();
  const BackgroundFetchRequestInfos& request_infos =
      BackgroundFetchJobDataTest::request_infos();
  ASSERT_EQ(10U, request_infos.size());

  // Start half of the fetch requests.
  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(job_data->HasRequestsRemaining());
    job_data->GetNextBackgroundFetchRequestInfo();
  }

  // Complete all of the fetches out of order except for #1.
  DownloadItem::DownloadState complete = DownloadItem::DownloadState::COMPLETE;
  DownloadInterruptReason no_interrupt =
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE;
  EXPECT_TRUE(job_data->UpdateBackgroundFetchRequestState(
      request_infos[3].guid(), complete, no_interrupt));
  EXPECT_TRUE(job_data->UpdateBackgroundFetchRequestState(
      request_infos[2].guid(), complete, no_interrupt));
  EXPECT_TRUE(job_data->UpdateBackgroundFetchRequestState(
      request_infos[4].guid(), complete, no_interrupt));
  EXPECT_TRUE(job_data->UpdateBackgroundFetchRequestState(
      request_infos[0].guid(), complete, no_interrupt));

  EXPECT_TRUE(job_data->HasRequestsRemaining());
  EXPECT_FALSE(job_data->IsComplete());

  // Start and complete the remaining requests.
  for (int i = 5; i < 10; i++) {
    job_data->GetNextBackgroundFetchRequestInfo();
    job_data->UpdateBackgroundFetchRequestState(request_infos[i].guid(),
                                                complete, no_interrupt);
  }

  EXPECT_FALSE(job_data->IsComplete());
  EXPECT_FALSE(job_data->HasRequestsRemaining());

  // Complete the final request.
  EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
      request_infos[1].guid(), complete, no_interrupt));
  EXPECT_TRUE(job_data->IsComplete());
}

TEST_F(BackgroundFetchJobDataTest, PauseAndResume) {
  std::unique_ptr<BackgroundFetchJobData> job_data = CreateMinimalJobData();
  const BackgroundFetchRequestInfos& request_infos =
      BackgroundFetchJobDataTest::request_infos();
  ASSERT_EQ(1U, request_infos.size());

  // Start the request.
  ASSERT_TRUE(job_data->HasRequestsRemaining());
  const BackgroundFetchRequestInfo& request_info =
      job_data->GetNextBackgroundFetchRequestInfo();

  EXPECT_FALSE(job_data->HasRequestsRemaining());
  EXPECT_FALSE(job_data->IsComplete());

  // Set the request state to be paused. This should not complete the job.
  EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
      request_info.guid(), DownloadItem::DownloadState::INTERRUPTED,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN));
  EXPECT_FALSE(job_data->IsComplete());

  // Unpause the request.
  EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
      request_info.guid(), DownloadItem::DownloadState::IN_PROGRESS,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_FALSE(job_data->IsComplete());

  // Complete the request.
  EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
      request_info.guid(), DownloadItem::DownloadState::COMPLETE,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE));
  EXPECT_TRUE(job_data->IsComplete());
}

TEST_F(BackgroundFetchJobDataTest, CancelledRequest) {
  std::unique_ptr<BackgroundFetchJobData> job_data = CreateMinimalJobData();
  const BackgroundFetchRequestInfos& request_infos =
      BackgroundFetchJobDataTest::request_infos();
  ASSERT_EQ(1U, request_infos.size());

  // Start the request.
  ASSERT_TRUE(job_data->HasRequestsRemaining());
  const BackgroundFetchRequestInfo& request_info =
      job_data->GetNextBackgroundFetchRequestInfo();

  EXPECT_FALSE(job_data->HasRequestsRemaining());
  EXPECT_FALSE(job_data->IsComplete());

  // Cancel the request.
  EXPECT_FALSE(job_data->UpdateBackgroundFetchRequestState(
      request_info.guid(), DownloadItem::DownloadState::CANCELLED,
      DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_TRUE(job_data->IsComplete());
}

}  // namespace content
