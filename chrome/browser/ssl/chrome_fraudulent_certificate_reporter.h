// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_
#define CHROME_BROWSER_SSL_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/url_request/fraudulent_certificate_reporter.h"

namespace chrome_browser_net {
class CertificateErrorReporter;
}  // namespace chrome_browser_net

namespace net {
class URLRequestContext;
}  // namespace net

class ChromeFraudulentCertificateReporter
    : public net::FraudulentCertificateReporter {
 public:
  explicit ChromeFraudulentCertificateReporter(
      net::URLRequestContext* request_context);

  // Useful for tests to use a mock reporter.
  explicit ChromeFraudulentCertificateReporter(scoped_ptr<
      chrome_browser_net::CertificateErrorReporter> certificate_reporter);

  ~ChromeFraudulentCertificateReporter() override;

  // net::FraudulentCertificateReporter
  void SendReport(const std::string& hostname,
                  const net::SSLInfo& ssl_info) override;

 private:
  scoped_ptr<chrome_browser_net::CertificateErrorReporter>
      certificate_reporter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFraudulentCertificateReporter);
};

#endif  // CHROME_BROWSER_SSL_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_
