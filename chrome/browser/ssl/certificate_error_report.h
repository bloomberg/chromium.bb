// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_
#define CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace net {
class SSLInfo;
}  // namespace net

class CertLoggerRequest;

// This class builds and serializes reports for invalid SSL certificate
// chains, intended to be sent with
// chrome_browser_net::CertificateErrorReporter.
class CertificateErrorReport {
 public:
  // Constructs an empty report.
  CertificateErrorReport();

  // Constructs a report for the given |hostname| using the SSL
  // properties in |ssl_info|.
  CertificateErrorReport(const std::string& hostname,
                         const net::SSLInfo& ssl_info);

  ~CertificateErrorReport();

  // Initializes an empty report by parsing the given serialized
  // report. |serialized_report| should be a serialized
  // CertLoggerRequest protobuf. Returns true if the report could be
  // successfully parsed and false otherwise.
  bool InitializeFromString(const std::string& serialized_report);

  // Serializes the report. The output will be a serialized
  // CertLoggerRequest protobuf. Returns true if the serialization was
  // successful and false otherwise.
  bool Serialize(std::string* output) const;

  // Gets the hostname to which this report corresponds.
  const std::string& hostname() const;

 private:
  scoped_ptr<CertLoggerRequest> cert_report_;
};

#endif  // CHROME_BROWSER_SSL_CERTIFICATE_ERROR_REPORT_H_
