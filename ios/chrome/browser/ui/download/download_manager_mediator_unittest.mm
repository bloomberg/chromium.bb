// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_mediator.h"

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/test/fakes/fake_download_manager_consumer.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher_response_writer.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::WaitUntilConditionOrTimeout;

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";
const int64_t kTestTotalBytes = 10;
const int64_t kTestReceivedBytes = 0;
NSString* const kTestSuggestedFileName = @"important_file.zip";

}  // namespace

// Test fixture for testing DownloadManagerMediator class.
class DownloadManagerMediatorTest : public PlatformTest {
 protected:
  DownloadManagerMediatorTest()
      : consumer_([[FakeDownloadManagerConsumer alloc] init]),
        task_(GURL(kTestUrl), kTestMimeType) {}

  web::FakeDownloadTask* task() { return &task_; }

  DownloadManagerMediator mediator_;
  FakeDownloadManagerConsumer* consumer_;

 private:
  web::TestWebThreadBundle thread_bundle_;
  web::FakeDownloadTask task_;
};

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into download directory.
TEST_F(DownloadManagerMediatorTest, Start) {
  task()->SetSuggestedFilename(
      base::SysNSStringToUTF16(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return task()->GetState() == web::DownloadTask::State::kInProgress;
  }));

  // Download file should be located in download directory.
  base::FilePath file =
      task()->GetResponseWriter()->AsFileWriter()->file_path();
  base::FilePath download_dir;
  ASSERT_TRUE(GetDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));

  mediator_.SetDownloadTask(nullptr);
}

// Tests starting and failing the download. Simulates download failure from
// inability to create a file writer.
TEST_F(DownloadManagerMediatorTest, StartFailure) {
  // Writer can not be created without file name, which will fail the download.
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Writer is created by a background task, so wait for failure.
  ASSERT_TRUE(WaitUntilConditionOrTimeout(testing::kWaitForDownloadTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return consumer_.state == kDownloadManagerStateFailed;
  }));

  mediator_.SetDownloadTask(nullptr);
}

// Tests that consumer is updated right after it's set.
TEST_F(DownloadManagerMediatorTest, ConsumerInstantUpdate) {
  task()->SetDone(true);
  task()->SetSuggestedFilename(
      base::SysNSStringToUTF16(kTestSuggestedFileName));
  task()->SetTotalBytes(kTestTotalBytes);
  task()->SetReceivedBytes(kTestReceivedBytes);

  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  EXPECT_EQ(kDownloadManagerStateSuceeded, consumer_.state);
  EXPECT_NSEQ(kTestSuggestedFileName, consumer_.fileName);
  EXPECT_EQ(kTestTotalBytes, consumer_.countOfBytesExpectedToReceive);
  EXPECT_EQ(kTestReceivedBytes, consumer_.countOfBytesReceived);

  mediator_.SetDownloadTask(nullptr);
}

// Tests that consumer changes the state to kDownloadManagerStateFailed if task
// competed with an error.
TEST_F(DownloadManagerMediatorTest, ConsumerFailedStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateFailed, consumer_.state);

  mediator_.SetDownloadTask(nullptr);
}

// Tests that consumer changes the state to kDownloadManagerStateSuceeded if
// task competed without an error.
TEST_F(DownloadManagerMediatorTest, ConsumerSuceededStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSuceeded, consumer_.state);

  mediator_.SetDownloadTask(nullptr);
}

// Tests that consumer changes the state to kDownloadManagerStateInProgress if
// the task has started.
TEST_F(DownloadManagerMediatorTest, ConsumerInProgressStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->Start(std::make_unique<net::URLFetcherStringWriter>());
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);

  mediator_.SetDownloadTask(nullptr);
}
