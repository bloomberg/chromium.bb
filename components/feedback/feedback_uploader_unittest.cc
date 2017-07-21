// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <stddef.h>

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader_chrome.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr char kReportOne[] = "one";
constexpr char kReportTwo[] = "two";
constexpr char kReportThree[] = "three";
constexpr char kReportFour[] = "four";
constexpr char kReportFive[] = "five";

constexpr base::TimeDelta kRetryDelayForTest =
    base::TimeDelta::FromMilliseconds(100);

class MockFeedbackUploader : public FeedbackUploaderChrome {
 public:
  MockFeedbackUploader(content::BrowserContext* context,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : FeedbackUploaderChrome(context, task_runner) {}
  ~MockFeedbackUploader() override {}

  void RunMessageLoop() {
    if (ProcessingComplete())
      return;
    run_loop_ = base::MakeUnique<base::RunLoop>();
    run_loop_->Run();
  }

  const std::map<std::string, unsigned int>& dispatched_reports() const {
    return dispatched_reports_;
  }
  void set_expected_reports(size_t value) { expected_reports_ = value; }
  void set_simulate_failure(bool value) { simulate_failure_ = value; }

 private:
  // FeedbackUploaderChrome:
  void DispatchReport(scoped_refptr<FeedbackReport> report) override {
    if (base::ContainsKey(dispatched_reports_, report->data()))
      dispatched_reports_[report->data()]++;
    else
      dispatched_reports_[report->data()] = 1;

    dispatched_reports_count_++;

    if (simulate_failure_)
      OnReportUploadFailure(report);
    else
      OnReportUploadSuccess();

    if (ProcessingComplete()) {
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  bool ProcessingComplete() {
    return (dispatched_reports_count_ >= expected_reports_);
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::map<std::string, unsigned int> dispatched_reports_;
  size_t dispatched_reports_count_ = 0;
  size_t expected_reports_ = 0;
  bool simulate_failure_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockFeedbackUploader);
};

std::unique_ptr<KeyedService> CreateFeedbackUploaderService(
    content::BrowserContext* context) {
  return base::MakeUnique<MockFeedbackUploader>(
      context, FeedbackUploaderFactory::CreateUploaderTaskRunner());
}

}  // namespace

class FeedbackUploaderTest : public testing::Test {
 public:
  FeedbackUploaderTest()
      : context_(base::MakeUnique<content::TestBrowserContext>()),
        prefs_(
            base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>()) {
    user_prefs::UserPrefs::Set(context_.get(), prefs_.get());
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        context_.get(), &CreateFeedbackUploaderService);

    FeedbackUploader::SetMinimumRetryDelayForTesting(kRetryDelayForTest);
    uploader_ = static_cast<MockFeedbackUploader*>(
        FeedbackUploaderFactory::GetForBrowserContext(context_.get()));
  }

  ~FeedbackUploaderTest() override {
    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        context_.get(), NULL);
  }

  void QueueReport(const std::string& data) {
    uploader_->QueueReport(data);
  }

  MockFeedbackUploader* uploader() const { return uploader_; }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<content::TestBrowserContext> context_;
  std::unique_ptr<PrefService> prefs_;

  MockFeedbackUploader* uploader_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderTest);
};

TEST_F(FeedbackUploaderTest, QueueMultiple) {
  QueueReport(kReportOne);
  QueueReport(kReportTwo);
  QueueReport(kReportThree);
  QueueReport(kReportFour);

  EXPECT_EQ(uploader()->dispatched_reports().size(), 4u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportOne), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportTwo), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportThree), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFour), 1u);
}

TEST_F(FeedbackUploaderTest, QueueMultipleWithFailures) {
  EXPECT_EQ(kRetryDelayForTest, uploader()->retry_delay());
  QueueReport(kReportOne);

  // Simulate a failure in reports two and three. Make sure the backoff delay
  // will be applied twice, and the reports will eventually be sent.
  uploader()->set_simulate_failure(true);
  QueueReport(kReportTwo);
  EXPECT_EQ(kRetryDelayForTest * 2, uploader()->retry_delay());
  QueueReport(kReportThree);
  EXPECT_EQ(kRetryDelayForTest * 4, uploader()->retry_delay());
  uploader()->set_simulate_failure(false);

  // Once a successful report is sent, the backoff delay is reset back to its
  // minimum value.
  QueueReport(kReportFour);
  EXPECT_EQ(kRetryDelayForTest, uploader()->retry_delay());
  QueueReport(kReportFive);

  // Wait for the pending two failed reports to be sent.
  uploader()->set_expected_reports(7);
  uploader()->RunMessageLoop();

  EXPECT_EQ(uploader()->dispatched_reports().size(), 5u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportOne), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportTwo), 2u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportThree), 2u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFour), 1u);
  EXPECT_EQ(uploader()->dispatched_reports().at(kReportFive), 1u);
}

}  // namespace feedback
