// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_uploader.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/feedback/feedback_uploader_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kReportOne[] = "one";
const char kReportTwo[] = "two";
const char kReportThree[] = "three";
const char kReportFour[] = "four";
const char kReportFive[] = "five";

const base::TimeDelta kRetryDelayForTest =
    base::TimeDelta::FromMilliseconds(100);

BrowserContextKeyedService* CreateFeedbackUploaderService(
    content::BrowserContext* context) {
  return new feedback::FeedbackUploader(Profile::FromBrowserContext(context));
}

}  // namespace

namespace feedback {

class FeedbackUploaderTest : public testing::Test {
 protected:
  FeedbackUploaderTest()
     : ui_thread_(content::BrowserThread::UI, &message_loop_),
       profile_(new TestingProfile()),
       expected_reports_(0) {
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        profile_.get(), &CreateFeedbackUploaderService);

    uploader_ = FeedbackUploaderFactory::GetForBrowserContext(profile_.get());
    uploader_->setup_for_test(
        base::Bind(&FeedbackUploaderTest::MockDispatchReport,
                   base::Unretained(this)),
        kRetryDelayForTest);
  }

  virtual ~FeedbackUploaderTest() {
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        profile_.get(), NULL);
  }

  void QueueReport(const std::string& data) {
    uploader_->QueueReport(make_scoped_ptr(new std::string(data)));
  }

  void ReportFailure(const std::string& data) {
    uploader_->RetryReport(make_scoped_ptr(new std::string(data)));
  }

  void MockDispatchReport(scoped_ptr<std::string> report_data) {
    dispatched_reports_.push_back(*report_data.get());

    // Dispatch will always update the timer, whether successful or not,
    // simulate the same behavior.
    uploader_->UpdateUploadTimer();

    if (dispatched_reports_.size() >= expected_reports_) {
      if (run_loop_.get())
        run_loop_->Quit();
    }
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;

  FeedbackUploader* uploader_;

  std::vector<std::string> dispatched_reports_;
  size_t expected_reports_;
};

TEST_F(FeedbackUploaderTest, QueueMultiple) {
  dispatched_reports_.clear();
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  EXPECT_EQ(dispatched_reports_.size(), 4u);
  EXPECT_EQ(dispatched_reports_[0], kReportOne);
  EXPECT_EQ(dispatched_reports_[1], kReportTwo);
  EXPECT_EQ(dispatched_reports_[2], kReportThree);
  EXPECT_EQ(dispatched_reports_[3], kReportFour);
}

#if defined(OS_WIN) || defined(OS_ANDROID)
// crbug.com/330547
#define MAYBE_QueueMultipleWithFailures DISABLED_QueueMultipleWithFailures
#else
#define MAYBE_QueueMultipleWithFailures QueueMultipleWithFailures
#endif
TEST_F(FeedbackUploaderTest, MAYBE_QueueMultipleWithFailures) {
  dispatched_reports_.clear();
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  ReportFailure(kReportThree);
  ReportFailure(kReportTwo);
  QueueReport(kReportFive);

  expected_reports_ = 7;
  RunMessageLoop();

  EXPECT_EQ(dispatched_reports_.size(), 7u);
  EXPECT_EQ(dispatched_reports_[0], kReportOne);
  EXPECT_EQ(dispatched_reports_[1], kReportTwo);
  EXPECT_EQ(dispatched_reports_[2], kReportThree);
  EXPECT_EQ(dispatched_reports_[3], kReportFour);
  EXPECT_EQ(dispatched_reports_[4], kReportFive);
  EXPECT_EQ(dispatched_reports_[5], kReportThree);
  EXPECT_EQ(dispatched_reports_[6], kReportTwo);
}

}  // namespace feedback
