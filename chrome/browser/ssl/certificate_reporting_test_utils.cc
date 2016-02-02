// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/certificate_reporting_test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/ping_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/browser/ssl/cert_report_helper.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_reporting/error_report.h"
#include "components/certificate_reporting/error_reporter.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "net/url_request/certificate_report_sender.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using safe_browsing::SafeBrowsingService;
using safe_browsing::SafeBrowsingUIManager;

namespace {

void SetMockReporter(
    SafeBrowsingService* safe_browsing_service,
    scoped_ptr<certificate_reporting::ErrorReporter> reporter) {
  safe_browsing_service->ping_manager()->SetCertificateErrorReporterForTesting(
      std::move(reporter));
}

// This is a test implementation of the interface that blocking pages
// use to send certificate reports. It checks that the blocking page
// calls or does not call the report method when a report should or
// should not be sent, respectively.
class MockSSLCertReporter : public SSLCertReporter {
 public:
  MockSSLCertReporter(
      const scoped_refptr<SafeBrowsingUIManager>& safe_browsing_ui_manager,
      const base::Closure& report_sent_callback)
      : safe_browsing_ui_manager_(safe_browsing_ui_manager),
        reported_(false),
        expect_report_(false),
        report_sent_callback_(report_sent_callback) {}

  ~MockSSLCertReporter() override { EXPECT_EQ(expect_report_, reported_); }

  // SSLCertReporter implementation.
  void ReportInvalidCertificateChain(
      const std::string& serialized_report) override {
    reported_ = true;
    if (expect_report_) {
      safe_browsing_ui_manager_->ReportInvalidCertificateChain(
          serialized_report, report_sent_callback_);
    }
  }

  void set_expect_report(bool expect_report) { expect_report_ = expect_report; }

 private:
  const scoped_refptr<SafeBrowsingUIManager> safe_browsing_ui_manager_;
  bool reported_;
  bool expect_report_;
  base::Closure report_sent_callback_;
};

}  // namespace

namespace certificate_reporting_test_utils {

// This class is used to test invalid certificate chain reporting when
// the user opts in to do so on the interstitial. It keeps track of the
// most recent hostname for which an extended reporting report would
// have been sent over the network.
class CertificateReportingTest::MockReporter
    : public certificate_reporting::ErrorReporter {
 public:
  MockReporter(
      net::URLRequestContext* request_context,
      const GURL& upload_url,
      net::CertificateReportSender::CookiesPreference cookies_preference);

  // ErrorReporter implementation.
  void SendExtendedReportingReport(
      const std::string& serialized_report) override;

  // Returns the hostname in the report for the last call to
  // |SendReport|.
  const std::string& latest_hostname_reported() {
    return latest_hostname_reported_;
  }

 private:
  std::string latest_hostname_reported_;

  DISALLOW_COPY_AND_ASSIGN(MockReporter);
};

CertificateReportingTest::MockReporter::MockReporter(
    net::URLRequestContext* request_context,
    const GURL& upload_url,
    net::CertificateReportSender::CookiesPreference cookies_preference)
    : certificate_reporting::ErrorReporter(request_context,
                                           upload_url,
                                           cookies_preference) {}

void CertificateReportingTest::MockReporter::SendExtendedReportingReport(
    const std::string& serialized_report) {
  certificate_reporting::ErrorReport report;
  ASSERT_TRUE(report.InitializeFromString(serialized_report));
  latest_hostname_reported_ = report.hostname();
}

void CertificateReportingTest::SetUpMockReporter() {
  // Set up the mock reporter to track the hostnames that reports get
  // sent for. The request_context argument is null here
  // because the MockReporter doesn't actually use a
  // request_context. (In order to pass a real request_context, the
  // reporter would have to be constructed on the IO thread.)
  reporter_ = new CertificateReportingTest::MockReporter(
      nullptr, GURL("http://example.test"),
      net::CertificateReportSender::DO_NOT_SEND_COOKIES);

  scoped_refptr<SafeBrowsingService> safe_browsing_service =
      g_browser_process->safe_browsing_service();
  ASSERT_TRUE(safe_browsing_service);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(SetMockReporter, safe_browsing_service,
                 base::Passed(scoped_ptr<certificate_reporting::ErrorReporter>(
                     reporter_))));
}

const std::string& CertificateReportingTest::GetLatestHostnameReported() const {
  return reporter_->latest_hostname_reported();
}

void SetCertReportingOptIn(Browser* browser, OptIn opt_in) {
  browser->profile()->GetPrefs()->SetBoolean(
      prefs::kSafeBrowsingExtendedReportingEnabled,
      opt_in == EXTENDED_REPORTING_OPT_IN);
}

scoped_ptr<SSLCertReporter> SetUpMockSSLCertReporter(
    base::RunLoop* run_loop,
    ExpectReport expect_report) {
  // Set up a MockSSLCertReporter to keep track of when the blocking
  // page invokes the cert reporter.
  SafeBrowsingService* sb_service = g_browser_process->safe_browsing_service();
  EXPECT_TRUE(sb_service);
  if (!sb_service)
    return nullptr;

  scoped_ptr<MockSSLCertReporter> ssl_cert_reporter(new MockSSLCertReporter(
      sb_service->ui_manager(), expect_report == CERT_REPORT_EXPECTED
                                    ? run_loop->QuitClosure()
                                    : base::Bind(&base::DoNothing)));
  ssl_cert_reporter->set_expect_report(expect_report == CERT_REPORT_EXPECTED);
  return std::move(ssl_cert_reporter);
}

ExpectReport GetReportExpectedFromFinch() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      CertReportHelper::kFinchExperimentName);

  if (group_name == CertReportHelper::kFinchGroupShowPossiblySend) {
    const std::string param = variations::GetVariationParamValue(
        CertReportHelper::kFinchExperimentName,
        CertReportHelper::kFinchParamName);
    double sendingThreshold;
    if (!base::StringToDouble(param, &sendingThreshold))
      return CERT_REPORT_NOT_EXPECTED;

    if (sendingThreshold == 1.0)
      return certificate_reporting_test_utils::CERT_REPORT_EXPECTED;
  }
  return certificate_reporting_test_utils::CERT_REPORT_NOT_EXPECTED;
}

}  // namespace certificate_reporting_test_utils
