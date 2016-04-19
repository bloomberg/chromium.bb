// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_REPORTING_TEST_UTILS_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/in_process_browser_test.h"

class Browser;
class GURL;
class SSLCertReporter;

namespace net {
class URLRequestContext;
}

namespace certificate_reporting_test_utils {

enum OptIn { EXTENDED_REPORTING_OPT_IN, EXTENDED_REPORTING_DO_NOT_OPT_IN };

enum ExpectReport { CERT_REPORT_EXPECTED, CERT_REPORT_NOT_EXPECTED };

// A test class that tracks the latest hostname for which a certificate
// report would have been sent over the network.
class CertificateReportingTest : public InProcessBrowserTest {
 public:
  // Set up the mock reporter that keeps track of certificate reports
  // that the safe browsing service sends.
  void SetUpMockReporter();

 protected:
  // Get the latest hostname for which a certificate report was
  // sent. SetUpMockReporter() must have been called before this.
  const std::string& GetLatestHostnameReported() const;

 private:
  class MockReporter;
  MockReporter* reporter_;
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
