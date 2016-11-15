// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_

#include <memory>
#include <string>

#include "components/certificate_reporting/error_reporter.h"

class Browser;
class GURL;
class SSLCertReporter;

namespace base {
class RunLoop;
}

namespace net {
class URLRequestContext;
}

namespace certificate_reporting_test_utils {

enum OptIn { EXTENDED_REPORTING_OPT_IN, EXTENDED_REPORTING_DO_NOT_OPT_IN };

enum ExpectReport { CERT_REPORT_EXPECTED, CERT_REPORT_NOT_EXPECTED };

// This class is used to test invalid certificate chain reporting when
// the user opts in to do so on the interstitial. It keeps track of the
// most recent hostname for which an extended reporting report would
// have been sent over the network.
class MockErrorReporter : public certificate_reporting::ErrorReporter {
 public:
  MockErrorReporter(net::URLRequestContext* request_context,
                    const GURL& upload_url,
                    net::ReportSender::CookiesPreference cookies_preference);

  // ErrorReporter implementation.
  void SendExtendedReportingReport(
      const std::string& serialized_report,
      const base::Callback<void()>& success_callback,
      const base::Callback<void(const GURL&, int)>& error_callback) override;

  // Returns the hostname in the report for the last call to |SendReport|.
  const std::string& latest_hostname_reported() {
    return latest_hostname_reported_;
  }

 private:
  std::string latest_hostname_reported_;

  DISALLOW_COPY_AND_ASSIGN(MockErrorReporter);
};

// A test class that tracks the latest hostname for which a certificate
// report would have been sent over the network.
class CertificateReportingTest {
 public:
  // Set up the mock reporter that keeps track of certificate reports
  // that the safe browsing service sends.
  void SetUpMockReporter();

 protected:
  // Get the latest hostname for which a certificate report was
  // sent. SetUpMockReporter() must have been called before this.
  const std::string& GetLatestHostnameReported() const;

 private:
  MockErrorReporter* reporter_;
};

// Sets the browser preference to enable or disable extended reporting.
void SetCertReportingOptIn(Browser* browser, OptIn opt_in);

// Sets up a mock SSLCertReporter and return a pointer to it, which will
// be owned by the caller. The mock SSLCertReporter will call
// |run_loop|'s QuitClosure when a report is sent. It also checks that a
// report is sent or not sent according to |expect_report|.
std::unique_ptr<SSLCertReporter> SetUpMockSSLCertReporter(
    base::RunLoop* run_loop,
    ExpectReport expect_report);

// Returns whether a report should be expected (due to the Finch config)
// if the user opts in.
ExpectReport GetReportExpectedFromFinch();

}  // namespace certificate_reporting_test_utils

#endif  // CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_
