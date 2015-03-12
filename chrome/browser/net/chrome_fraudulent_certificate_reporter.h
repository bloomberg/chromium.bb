// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_
#define CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "net/url_request/fraudulent_certificate_reporter.h"

namespace net {
class URLRequestContext;
}

namespace chrome_browser_net {

class CertificateErrorReporter;

class ChromeFraudulentCertificateReporter
    : public net::FraudulentCertificateReporter {
 public:
  explicit ChromeFraudulentCertificateReporter(
      net::URLRequestContext* request_context);

  // Useful for tests to use a mock reporter.
  explicit ChromeFraudulentCertificateReporter(
      scoped_ptr<CertificateErrorReporter> certificate_reporter);

  ~ChromeFraudulentCertificateReporter() override;

  // net::FraudulentCertificateReporter
  void SendReport(const std::string& hostname,
                  const net::SSLInfo& ssl_info) override;

 private:
  scoped_ptr<CertificateErrorReporter> certificate_reporter_;

  DISALLOW_COPY_AND_ASSIGN(ChromeFraudulentCertificateReporter);
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_CHROME_FRAUDULENT_CERTIFICATE_REPORTER_H_
