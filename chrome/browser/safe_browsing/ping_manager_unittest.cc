// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "base/base64.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "components/certificate_reporting/error_reporter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using safe_browsing::HitReport;
using safe_browsing::ThreatSource;

static const char kUrlPrefix[] = "https://prefix.com/foo";
static const char kClient[] = "unittest";
static const char kAppVer[] = "1.0";
// Histogram for certificate reporting failures:
static const char kFailureHistogramName[] = "SSL.CertificateErrorReportFailure";

namespace safe_browsing {

class SafeBrowsingPingManagerTest : public testing::Test {
 public:
  SafeBrowsingPingManagerTest()
      : net_log_(new net::TestNetLog()) {
    net_log_with_source_ = net::NetLogWithSource::Make(
        net_log_.get(), net::NetLogSourceType::SAFE_BROWSING);
  }

 protected:
  void SetUp() override {
    std::string key = google_apis::GetAPIKey();
    if (!key.empty()) {
      key_param_ = base::StringPrintf(
          "&key=%s",
          net::EscapeQueryParamValue(key, true).c_str());
    }

    SafeBrowsingProtocolConfig config;
    config.client_name = kClient;
    config.url_prefix = kUrlPrefix;
    ping_manager_.reset(new SafeBrowsingPingManager(NULL, config));
    ping_manager_->version_ = kAppVer;
    ping_manager_->net_log_ = net_log_with_source_;
  }

  SafeBrowsingPingManager* ping_manager()  {
    return ping_manager_.get();
  }

  std::string key_param_;
  std::unique_ptr<net::TestNetLog> net_log_;
  net::NetLogWithSource net_log_with_source_;
  net::TestURLFetcherFactory fetcher_factory_;
  std::unique_ptr<SafeBrowsingPingManager> ping_manager_;
};

TEST_F(SafeBrowsingPingManagerTest, TestSafeBrowsingHitUrl) {
  HitReport base_hp;
  base_hp.malicious_url = GURL("http://malicious.url.com");
  base_hp.page_url = GURL("http://page.url.com");
  base_hp.referrer_url = GURL("http://referrer.url.com");

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_MALWARE;
    hp.threat_source = ThreatSource::LOCAL_PVER3;
    hp.is_subresource = true;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;

    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=1&evts=malblhit&evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l3&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::DATA_SAVER;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_LEGACY;
    hp.is_metrics_reporting_active = true;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=1&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=ds&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_URL_PHISHING;
    hp.threat_source = ThreatSource::DATA_SAVER;
    hp.is_subresource = false;
    hp.extended_reporting_level = SBER_LEVEL_SCOUT;
    hp.is_metrics_reporting_active = true;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=2&evts=phishblhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=ds&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_BINARY_MALWARE_URL;
    hp.threat_source = ThreatSource::REMOTE;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = true;
    hp.is_subresource = false;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=binurlhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=rem&m=1",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_PHISHING_URL;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = false;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=phishcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=0&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }

  // Same as above, but add population_id
  {
    HitReport hp(base_hp);
    hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
    hp.threat_source = ThreatSource::LOCAL_PVER4;
    hp.extended_reporting_level = SBER_LEVEL_OFF;
    hp.is_metrics_reporting_active = false;
    hp.is_subresource = true;
    hp.population_id = "foo bar";
    EXPECT_EQ(
        "https://prefix.com/foo/report?client=unittest&appver=1.0&"
        "pver=3.0" +
            key_param_ +
            "&ext=0&evts=malcsdhit&"
            "evtd=http%3A%2F%2Fmalicious.url.com%2F&"
            "evtr=http%3A%2F%2Fpage.url.com%2F&evhr=http%3A%2F%2Freferrer."
            "url.com%2F&evtb=1&src=l4&m=0&up=foo+bar",
        ping_manager()->SafeBrowsingHitUrl(hp).spec());
  }
}

TEST_F(SafeBrowsingPingManagerTest, TestThreatDetailsUrl) {
  EXPECT_EQ("https://prefix.com/foo/clientreport/malware?"
            "client=unittest&appver=1.0&pver=1.0" + key_param_,
            ping_manager()->ThreatDetailsUrl().spec());
}

TEST_F(SafeBrowsingPingManagerTest, TestReportThreatDetails) {
  const std::string kThreatDetailsReportString = "Threat Details Report String";
  std::string encoded_threat_report = "";
  base::Base64Encode(kThreatDetailsReportString, &encoded_threat_report);
  std::string expected_threat_details_url = ping_manager()->ThreatDetailsUrl()
      .spec();
  const int kRequestErrorCode = -123;

  // Start the report.
  ping_manager()->ReportThreatDetails(kThreatDetailsReportString);

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  DCHECK(fetcher);
  // Set some error response data on the fetcher to make things interesting.
  fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, kRequestErrorCode));
  // Tell the test fetcher to invoke the fetch callback.
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // We expect two net log entries: one when the ping starts, one when it ends.
  net::TestNetLogEntry::List entries;
  net_log_->GetEntries(&entries);
  ASSERT_EQ(2u, entries.size());

  // Check for expected log entries for the begin phase.
  const net::TestNetLogEntry& start_entry = entries[0];
  ASSERT_EQ(3u, start_entry.params->size());

  std::string string_value;
  EXPECT_TRUE(start_entry.GetStringValue("url", &string_value));
  EXPECT_EQ(expected_threat_details_url, string_value);

  EXPECT_TRUE(start_entry.GetStringValue("payload", &string_value));
  EXPECT_EQ(encoded_threat_report, string_value);

  // We don't really care what the source_dependency value is, just making sure
  // it's there.
  EXPECT_TRUE(start_entry.params->HasKey("source_dependency"));

  // Check for expected log entries for the end phase.
  const net::TestNetLogEntry& end_entry = entries[1];
  ASSERT_EQ(3u, end_entry.params->size());

  int int_value;
  EXPECT_TRUE(end_entry.GetIntegerValue("status", &int_value));
  EXPECT_EQ(net::URLRequestStatus::FAILED, int_value);

  EXPECT_TRUE(end_entry.GetIntegerValue("error", &int_value));
  EXPECT_EQ(kRequestErrorCode, int_value);

  // We don't really care what the source_dependency value is, just making sure
  // it's there.
  EXPECT_TRUE(end_entry.params->HasKey("source_dependency"));
}

TEST_F(SafeBrowsingPingManagerTest, TestReportSafeBrowsingHit) {
  const std::string kHitReportPostData = "Hit Report POST Data";
  std::string encoded_post_data = "";
  base::Base64Encode(kHitReportPostData, &encoded_post_data);

  HitReport hp;
  hp.malicious_url = GURL("http://malicious.url.com");
  hp.page_url = GURL("http://page.url.com");
  hp.referrer_url = GURL("http://referrer.url.com");
  hp.threat_type = SB_THREAT_TYPE_CLIENT_SIDE_MALWARE_URL;
  hp.threat_source = ThreatSource::LOCAL_PVER4;
  hp.extended_reporting_level = SBER_LEVEL_OFF;
  hp.is_metrics_reporting_active = false;
  hp.is_subresource = true;
  hp.population_id = "foo bar";
  hp.post_data = kHitReportPostData;
  std::string expected_hit_report_url = ping_manager()->SafeBrowsingHitUrl(hp)
      .spec();
  const int kRequestErrorCode = -321;

  // Start the report.
  ping_manager()->ReportSafeBrowsingHit(hp);

  net::TestURLFetcher* fetcher = fetcher_factory_.GetFetcherByID(0);
  DCHECK(fetcher);
  // Set some error response data on the fetcher to make things interesting.
  fetcher->set_status(
      net::URLRequestStatus(net::URLRequestStatus::FAILED, kRequestErrorCode));
  // Tell the test fetcher to invoke the fetch callback.
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // We expect two net log entries: one when the ping starts, one when it ends.
  net::TestNetLogEntry::List entries;
  net_log_->GetEntries(&entries);
  ASSERT_EQ(2u, entries.size());

  // Check for expected log entries for the begin phase.
  const net::TestNetLogEntry& start_entry = entries[0];
  ASSERT_EQ(3u, start_entry.params->size());

  std::string string_value;
  EXPECT_TRUE(start_entry.GetStringValue("url", &string_value));
  EXPECT_EQ(expected_hit_report_url, string_value);

  EXPECT_TRUE(start_entry.GetStringValue("payload", &string_value));
  EXPECT_EQ(encoded_post_data, string_value);

  // We don't really care what the source_dependency value is, just making sure
  // it's there.
  EXPECT_TRUE(start_entry.params->HasKey("source_dependency"));

  // Check for expected log entries for the end phase.
  const net::TestNetLogEntry& end_entry = entries[1];
  ASSERT_EQ(3u, end_entry.params->size());

  int int_value;
  EXPECT_TRUE(end_entry.GetIntegerValue("status", &int_value));
  EXPECT_EQ(net::URLRequestStatus::FAILED, int_value);

  EXPECT_TRUE(end_entry.GetIntegerValue("error", &int_value));
  EXPECT_EQ(kRequestErrorCode, int_value);

  // We don't really care what the source_dependency value is, just making sure
  // it's there.
  EXPECT_TRUE(end_entry.params->HasKey("source_dependency"));
}

class SafeBrowsingPingManagerCertReportingTest : public testing::Test {
 public:
  SafeBrowsingPingManagerCertReportingTest() {}

 protected:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoopForIO());
    io_thread_.reset(new content::TestBrowserThread(content::BrowserThread::IO,
                                                    message_loop_.get()));
    url_request_context_getter_ =
        new net::TestURLRequestContextGetter(message_loop_->task_runner());
    net::URLRequestFailedJob::AddUrlHandler();
  }

  net::URLRequestContextGetter* url_request_context_getter() {
    return url_request_context_getter_.get();
  }

 private:
  std::unique_ptr<base::MessageLoopForIO> message_loop_;
  std::unique_ptr<content::TestBrowserThread> io_thread_;

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
};

TEST_F(SafeBrowsingPingManagerCertReportingTest, UMAOnFailure) {
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kFailureHistogramName, 0);

  SafeBrowsingProtocolConfig config;
  config.client_name = kClient;
  config.url_prefix = kUrlPrefix;
  std::unique_ptr<SafeBrowsingPingManager> ping_manager(
      new SafeBrowsingPingManager(url_request_context_getter(), config));
  const GURL kReportURL =
      net::URLRequestFailedJob::GetMockHttpsUrl(net::ERR_CONNECTION_FAILED);

  ping_manager->certificate_error_reporter_->set_upload_url_for_testing(
      kReportURL);
  ping_manager->ReportInvalidCertificateChain("report");
  base::RunLoop().RunUntilIdle();

  histograms.ExpectTotalCount(kFailureHistogramName, 1);
  histograms.ExpectBucketCount(kFailureHistogramName,
                               -net::ERR_CONNECTION_FAILED, 1);
}

}  // namespace safe_browsing
