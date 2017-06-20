// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CHROME_EXPECT_CT_REPORTER_H_
#define CHROME_BROWSER_SSL_CHROME_EXPECT_CT_REPORTER_H_

#include <memory>

#include "base/macros.h"
#include "net/http/transport_security_state.h"

namespace net {
class ReportSender;
class URLRequestContext;
}  // namespace net

// This class monitors for violations of CT policy and sends reports
// about failures for sites that have opted in. Must be deleted before
// the URLRequestContext that is passed to the constructor, so that it
// can cancel its requests.
class ChromeExpectCTReporter
    : public net::TransportSecurityState::ExpectCTReporter {
 public:
  explicit ChromeExpectCTReporter(net::URLRequestContext* request_context);
  ~ChromeExpectCTReporter() override;

  // net::ExpectCTReporter:
  void OnExpectCTFailed(const net::HostPortPair& host_port_pair,
                        const GURL& report_uri,
                        base::Time expiration,
                        const net::X509Certificate* validated_certificate_chain,
                        const net::X509Certificate* served_certificate_chain,
                        const net::SignedCertificateTimestampAndStatusList&
                            signed_certificate_timestamps) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeExpectCTReporterTest, FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(ChromeExpectCTReporterTest, EmptyReportURI);
  FRIEND_TEST_ALL_PREFIXES(ChromeExpectCTReporterTest, SendReport);

  std::unique_ptr<net::ReportSender> report_sender_;

  DISALLOW_COPY_AND_ASSIGN(ChromeExpectCTReporter);
};

#endif  // CHROME_BROWSER_SSL_CHROME_EXPECT_CT_REPORTER_H_
