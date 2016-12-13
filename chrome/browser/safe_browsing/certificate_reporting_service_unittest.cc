// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/thread_test_helper.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_delegate_impl.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Maximum number of reports kept in the certificate reporting service's retry
// queue.
const size_t kMaxReportCountInQueue = 3;

void ClearURLHandlers() {
  net::URLRequestFilter::GetInstance()->ClearHandlers();
}

}  // namespace

TEST(CertificateReportingServiceReportListTest, BoundedReportList) {
  // Create a report list with maximum of 2 items.
  CertificateReportingService::BoundedReportList list(2 /* max_size */);
  EXPECT_EQ(0u, list.items().size());

  // Add a ten minute old report.
  list.Add(CertificateReportingService::Report(
      1, base::Time::Now() - base::TimeDelta::FromMinutes(10),
      std::string("report1_ten_minutes_old")));
  EXPECT_EQ(1u, list.items().size());
  EXPECT_EQ("report1_ten_minutes_old", list.items()[0].serialized_report);

  // Add another report. Items are ordered from newest to oldest report by
  // creation time. Oldest is at the end.
  list.Add(CertificateReportingService::Report(
      2, base::Time::Now() - base::TimeDelta::FromMinutes(5),
      std::string("report2_five_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report2_five_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report1_ten_minutes_old", list.items()[1].serialized_report);

  // Adding a third report removes the oldest report (report1) from the list.
  list.Add(CertificateReportingService::Report(
      3, base::Time::Now(), std::string("report3_zero_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report3_zero_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report2_five_minutes_old", list.items()[1].serialized_report);

  // Adding a report older than the oldest report in the list (report2) is
  // a no-op.
  list.Add(CertificateReportingService::Report(
      0, base::Time::Now() - base::TimeDelta::FromMinutes(
                                 10) /* 5 minutes older than report2 */,
      std::string("report0_ten_minutes_old")));
  EXPECT_EQ(2u, list.items().size());
  EXPECT_EQ("report3_zero_minutes_old", list.items()[0].serialized_report);
  EXPECT_EQ("report2_five_minutes_old", list.items()[1].serialized_report);
}

class CertificateReportingServiceReporterOnIOThreadTest
    : public ::testing::Test {
 public:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoopForIO());
    io_thread_.reset(new content::TestBrowserThread(content::BrowserThread::IO,
                                                    message_loop_.get()));
    url_request_context_getter_ =
        new net::TestURLRequestContextGetter(message_loop_->task_runner());
    net::URLRequestFailedJob::AddUrlHandler();
    net::URLRequestMockDataJob::AddUrlHandler();
  }

  void TearDown() override { ClearURLHandlers(); }

 protected:
  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }

 private:
  std::unique_ptr<base::MessageLoopForIO> message_loop_;
  std::unique_ptr<content::TestBrowserThread> io_thread_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
};

TEST_F(CertificateReportingServiceReporterOnIOThreadTest,
       Reporter_RetriesEnabled) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);

  const GURL kFailureURL =
      net::URLRequestFailedJob::GetMockHttpsUrl(net::ERR_SSL_PROTOCOL_ERROR);
  certificate_reporting::ErrorReporter* certificate_error_reporter =
      new certificate_reporting::ErrorReporter(
          url_request_context_getter()->GetURLRequestContext(), kFailureURL,
          net::ReportSender::DO_NOT_SEND_COOKIES);

  CertificateReportingService::BoundedReportList* list =
      new CertificateReportingService::BoundedReportList(2);

  // Create a reporter with retries enabled.
  CertificateReportingService::Reporter reporter(
      std::unique_ptr<certificate_reporting::ErrorReporter>(
          certificate_error_reporter),
      std::unique_ptr<CertificateReportingService::BoundedReportList>(list),
      clock.get(), base::TimeDelta::FromSeconds(100),
      true /* retries_enabled */);
  EXPECT_EQ(0u, list->items().size());
  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());

  // Sending a failed report will put the report in the retry list.
  reporter.Send("report1");
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());
  ASSERT_EQ(1u, list->items().size());
  EXPECT_EQ("report1", list->items()[0].serialized_report);
  EXPECT_EQ(reference_time, list->items()[0].creation_time);

  // Sending a second failed report will also put it in the retry list.
  clock->Advance(base::TimeDelta::FromSeconds(10));
  reporter.Send("report2");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  EXPECT_EQ("report2", list->items()[0].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(10),
            list->items()[0].creation_time);
  EXPECT_EQ("report1", list->items()[1].serialized_report);
  EXPECT_EQ(reference_time, list->items()[1].creation_time);

  // Sending a third report should remove the first report from the retry list.
  clock->Advance(base::TimeDelta::FromSeconds(10));
  reporter.Send("report3");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  EXPECT_EQ("report3", list->items()[0].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(20),
            list->items()[0].creation_time);
  EXPECT_EQ("report2", list->items()[1].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(10),
            list->items()[1].creation_time);

  // Retry sending all pending reports. All should fail and get added to the
  // retry list again. Report creation time doesn't change for retried reports.
  clock->Advance(base::TimeDelta::FromSeconds(10));
  reporter.SendPending();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, list->items().size());
  EXPECT_EQ("report3", list->items()[0].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(20),
            list->items()[0].creation_time);
  EXPECT_EQ("report2", list->items()[1].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(10),
            list->items()[1].creation_time);

  // Advance the clock to 115 seconds. This makes report3 95 seconds old, and
  // report2 105 seconds old. report2 should be dropped because it's older than
  // max age (100 seconds).
  clock->SetNow(reference_time + base::TimeDelta::FromSeconds(115));
  reporter.SendPending();
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1u, list->items().size());
  EXPECT_EQ("report3", list->items()[0].serialized_report);
  EXPECT_EQ(reference_time + base::TimeDelta::FromSeconds(20),
            list->items()[0].creation_time);

  // Retry again, this time successfully. There should be no pending reports
  // left.
  const GURL kSuccessURL =
      net::URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  certificate_error_reporter->set_upload_url_for_testing(kSuccessURL);
  clock->Advance(base::TimeDelta::FromSeconds(1));
  reporter.SendPending();
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, list->items().size());
}

// Same as above, but retries are disabled.
TEST_F(CertificateReportingServiceReporterOnIOThreadTest,
       Reporter_RetriesDisabled) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);

  const GURL kFailureURL =
      net::URLRequestFailedJob::GetMockHttpsUrl(net::ERR_SSL_PROTOCOL_ERROR);
  certificate_reporting::ErrorReporter* certificate_error_reporter =
      new certificate_reporting::ErrorReporter(
          url_request_context_getter()->GetURLRequestContext(), kFailureURL,
          net::ReportSender::DO_NOT_SEND_COOKIES);

  CertificateReportingService::BoundedReportList* list =
      new CertificateReportingService::BoundedReportList(2);

  // Create a reporter with retries disabled.
  CertificateReportingService::Reporter reporter(
      std::unique_ptr<certificate_reporting::ErrorReporter>(
          certificate_error_reporter),
      std::unique_ptr<CertificateReportingService::BoundedReportList>(list),
      clock.get(), base::TimeDelta::FromSeconds(100),
      false /* retries_enabled */);
  EXPECT_EQ(0u, list->items().size());
  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());

  // Sending a failed report will not put the report in the retry list.
  reporter.Send("report1");
  EXPECT_EQ(1u, reporter.inflight_report_count_for_testing());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, reporter.inflight_report_count_for_testing());
  ASSERT_EQ(0u, list->items().size());

  // Sending a second failed report will also not put it in the retry list.
  clock->Advance(base::TimeDelta::FromSeconds(10));
  reporter.Send("report2");
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, list->items().size());

  // Send pending reports. Nothing should be sent.
  clock->Advance(base::TimeDelta::FromSeconds(10));
  reporter.SendPending();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, list->items().size());
}

class CertificateReportingServiceTest
    : public ::testing::Test,
      public certificate_reporting_test_utils::
          CertificateReportingServiceTestBase {
 public:
  CertificateReportingServiceTest()
      : CertificateReportingServiceTestBase(),
        thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD),
        io_task_runner_(content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO)) {}

  ~CertificateReportingServiceTest() override {}

  void SetUp() override {
    SetUpInterceptor();
    WaitForIOThread();

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(
            &CertificateReportingServiceTest::SetUpURLRequestContextOnIOThread,
            base::Unretained(this)));
    WaitForIOThread();

    clock_ = new base::SimpleTestClock();
    service_.reset(new CertificateReportingService(
        url_request_context_getter(), server_public_key(),
        server_public_key_version(), kMaxReportCountInQueue,
        base::TimeDelta::FromHours(24), std::unique_ptr<base::Clock>(clock_)));
    // Wait for service reset.
    WaitForIOThread();
  }

  void SetUpURLRequestContextOnIOThread() {
    std::unique_ptr<net::TestURLRequestContext> url_request_context(
        new net::TestURLRequestContext(true));
    url_request_context->set_network_delegate(network_delegate());
    url_request_context->Init();
    url_request_context_getter_ = new net::TestURLRequestContextGetter(
        io_task_runner_, std::move(url_request_context));
  }

  void TearDown() override {
    WaitForIOThread();
    EXPECT_TRUE(interceptor()->successful_reports().empty());
    EXPECT_TRUE(interceptor()->failed_reports().empty());
    EXPECT_TRUE(interceptor()->delayed_reports().empty());
    EXPECT_EQ(0u, service()
                      ->GetReporterForTesting()
                      ->inflight_report_count_for_testing());

    service_->Shutdown();
    WaitForIOThread();

    service_.reset(nullptr);
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::Bind(&ClearURLHandlers));
    TearDownInterceptor();
  }

 protected:
  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }

  void WaitForIOThread() {
    scoped_refptr<base::ThreadTestHelper> io_helper(
        new base::ThreadTestHelper(io_task_runner_));
    ASSERT_TRUE(io_helper->Run());
  }

  // Sets service enabled state and waits for a reset event.
  void SetServiceEnabledAndWait(bool enabled) {
    service()->SetEnabled(enabled);
    WaitForIOThread();
  }

  void AdvanceClock(base::TimeDelta delta) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&base::SimpleTestClock::Advance, base::Unretained(clock_),
                   delta));
  }

  void SetNow(base::Time now) {
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::Bind(&base::SimpleTestClock::SetNow,
                                                base::Unretained(clock_), now));
  }

  CertificateReportingService* service() { return service_.get(); }

 private:
  // Must be initialized before url_request_context_getter_
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  std::unique_ptr<CertificateReportingService> service_;
  base::SimpleTestClock* clock_;
};

TEST_F(CertificateReportingServiceTest, Send) {
  WaitForIOThread();
  // Let all reports fail.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send two reports. Both should fail and get queued.
  service()->Send("report0");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report0"}));

  service()->Send("report1");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report1"}));

  // Send pending reports. Previously queued reports should be observed. They
  // will also be queued again.
  service()->SendPending();
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report0", "report1"}));

  // Let all reports succeed.
  SetFailureMode(certificate_reporting_test_utils::ReportSendingResult::
                     REPORTS_SUCCESSFUL);

  // Send a third report. This should not be queued.
  service()->Send("report2");
  WaitForRequestsDestroyed(ReportExpectation::Successful({"report2"}));

  // Send pending reports. Previously failed and queued two reports should be
  // observed.
  service()->SendPending();
  WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report0", "report1"}));
}

TEST_F(CertificateReportingServiceTest, Disabled_ShouldNotSend) {
  // Let all reports succeed.
  SetFailureMode(certificate_reporting_test_utils::ReportSendingResult::
                     REPORTS_SUCCESSFUL);

  // Disable the service.
  SetServiceEnabledAndWait(false);

  // Send a report. Report attempt should be cancelled and no sent reports
  // should be observed.
  service()->Send("report0");

  // Enable the service and send a report again.
  SetServiceEnabledAndWait(true);

  service()->Send("report1");
  WaitForRequestsDestroyed(ReportExpectation::Successful({"report1"}));
}

TEST_F(CertificateReportingServiceTest, Disabled_ShouldClearPendingReports) {
  // Let all reports fail.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  service()->Send("report0");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report0"}));

  // Disable the service.
  SetServiceEnabledAndWait(false);

  // Sending has no effect while disabled, wait for a single cancelled event.
  service()->SendPending();

  // Re-enable the service and send pending reports. Pending reports should have
  // been cleared when the service was disabled, so no report should be seen.
  SetServiceEnabledAndWait(true);

  // Sending with empty queue has no effect.
  service()->SendPending();
}

TEST_F(CertificateReportingServiceTest, DontSendOldReports) {
  SetNow(base::Time::Now());
  // Let all reports fail.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a report.
  service()->Send("report0");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report0"}));

  // Advance the clock a bit and trigger another report.
  AdvanceClock(base::TimeDelta::FromHours(5));

  service()->Send("report1");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report1"}));

  // Advance the clock to 20 hours, putting it 25 hours ahead of the reference
  // time. This makes the report0 older than max age (24 hours). The report1 is
  // now 20 hours old.
  AdvanceClock(base::TimeDelta::FromHours(20));
  // Send pending reports. report0 should be discarded since it's too old.
  // report1 should be queued again.
  service()->SendPending();
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report1"}));

  // Send a third report.
  service()->Send("report2");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report2"}));

  // Advance the clock 5 hours. The report1 will now be 25 hours old.
  AdvanceClock(base::TimeDelta::FromHours(5));
  // Send pending reports. report1 should be discarded since it's too old.
  // report2 should be queued again.
  service()->SendPending();
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report2"}));

  // Advance the clock 20 hours again so that report2 is 25 hours old and is
  // older than max age (24 hours)
  AdvanceClock(base::TimeDelta::FromHours(20));
  // Send pending reports. report2 should be discarded since it's too old. No
  // other reports remain.
  service()->SendPending();
}

TEST_F(CertificateReportingServiceTest, DiscardOldReports) {
  SetNow(base::Time::Now());
  // Let all reports fail.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  service()->Send("report0");
  WaitForRequestsDestroyed(ReportExpectation::Failed({"report0"}));

  // Send three more reports within five hours of each other. After this:
  // report0 is 0 hours after reference time (15 hours old).
  // report1 is 5 hours after reference time (10 hours old).
  // report2 is 10 hours after reference time (5 hours old).
  // report3 is 15 hours after reference time (0 hours old).
  AdvanceClock(base::TimeDelta::FromHours(5));
  service()->Send("report1");

  AdvanceClock(base::TimeDelta::FromHours(5));
  service()->Send("report2");

  AdvanceClock(base::TimeDelta::FromHours(5));
  service()->Send("report3");
  WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1", "report2", "report3"}));

  // Send pending reports. Four reports were generated above, but the service
  // only queues three reports, so the very first one should be dropped since
  // it's the oldest.
  service()->SendPending();
  WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1", "report2", "report3"}));

  // Let all reports succeed.
  SetFailureMode(certificate_reporting_test_utils::ReportSendingResult::
                     REPORTS_SUCCESSFUL);

  // Advance the clock by 15 hours. Current time is now 30 hours after reference
  // time. The ages of reports are now as follows:
  // report1 is 25 hours old.
  // report2 is 20 hours old.
  // report3 is 15 hours old.
  AdvanceClock(base::TimeDelta::FromHours(15));
  // Send pending reports. Only report2 and report3 should be sent, report1
  // should be ignored because it's too old.
  service()->SendPending();
  WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report2", "report3"}));

  // Do a final send. No reports should be sent.
  service()->SendPending();
}

// A delayed report should successfully upload when it's resumed.
TEST_F(CertificateReportingServiceTest, Delayed_Resumed) {
  // Let reports hang.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);
  // Send a report. The report upload hangs, so no error or success callbacks
  // should be called.
  service()->Send("report0");

  // Resume the report upload and run the callbacks. The report should be
  // successfully sent.
  ResumeDelayedRequest();
  WaitForRequestsDestroyed(ReportExpectation::Delayed({"report0"}));
}

// Delayed reports should cleaned when the service is reset.
TEST_F(CertificateReportingServiceTest, Delayed_Reset) {
  // Let reports hang.
  SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);
  // Send a report. The report is triggered but hangs, so no error or success
  // callbacks should be called.
  service()->Send("report0");

  // Disable the service. This should reset the reporting service and
  // clear all pending reports.
  SetServiceEnabledAndWait(false);

  // Resume delayed report. No report should be observed since the service
  // should have reset and all pending reports should be cleared. If any report
  // is observed, the next WaitForRequestsDestroyed() will fail.
  ResumeDelayedRequest();

  // Enable the service.
  SetServiceEnabledAndWait(true);

  // Send a report. The report is triggered but hangs, so no error or success
  // callbacks should be called. The report id is again 0 since the pending
  // report queue has been cleared above.
  service()->Send("report1");

  // Resume delayed report. The report should be observed.
  ResumeDelayedRequest();
  WaitForRequestsDestroyed(ReportExpectation::Delayed({"report0", "report1"}));
}
