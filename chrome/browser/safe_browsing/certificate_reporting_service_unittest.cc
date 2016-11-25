// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/network_delegate_impl.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(CertificateReportingServiceTest, BoundedReportList) {
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

class CertificateReportingServiceReporterTest : public ::testing::Test {
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

 protected:
  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }

 private:
  std::unique_ptr<base::MessageLoopForIO> message_loop_;
  std::unique_ptr<content::TestBrowserThread> io_thread_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
};

TEST_F(CertificateReportingServiceReporterTest, Reporter) {
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

  CertificateReportingService::Reporter reporter(
      std::unique_ptr<certificate_reporting::ErrorReporter>(
          certificate_error_reporter),
      std::unique_ptr<CertificateReportingService::BoundedReportList>(list),
      clock.get(), base::TimeDelta::FromSeconds(100));
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
