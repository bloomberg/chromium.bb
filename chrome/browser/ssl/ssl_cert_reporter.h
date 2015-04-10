// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CERT_REPORTER_H_
#define CHROME_BROWSER_SSL_SSL_CERT_REPORTER_H_

#include <string>

namespace net {
class SSLInfo;
}  // namespace net

// An interface used by interstitial pages to send reports of invalid
// certificate chains.
class SSLCertReporter {
 public:
  virtual ~SSLCertReporter() {}

  // Send a report for |hostname| with the given |ssl_info| to the
  // report collection endpoint. |callback| will be run when the report has
  // been sent off (not necessarily after a response has been received).
  virtual void ReportInvalidCertificateChain(const std::string& hostname,
                                             const net::SSLInfo& ssl_info) = 0;
};

#endif  // CHROME_BROWSER_SSL_SSL_CERT_REPORTER_H_
