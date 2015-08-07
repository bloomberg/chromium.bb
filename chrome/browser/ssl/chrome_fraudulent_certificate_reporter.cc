// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_fraudulent_certificate_reporter.h"

#include "base/profiler/scoped_tracker.h"
#include "chrome/browser/net/certificate_error_reporter.h"
#include "chrome/browser/ssl/certificate_error_report.h"
#include "net/ssl/ssl_info.h"
#include "net/url_request/certificate_report_sender.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace {

// TODO(palmer): Switch to HTTPS when the error handling delegate is more
// sophisticated. Ultimately we plan to attempt the report on many transports.
const char kFraudulentCertificateUploadEndpoint[] =
    "http://clients3.google.com/log_cert_error";

}  // namespace

ChromeFraudulentCertificateReporter::ChromeFraudulentCertificateReporter(
    net::URLRequestContext* request_context)
    : certificate_reporter_(new chrome_browser_net::CertificateErrorReporter(
          request_context,
          GURL(kFraudulentCertificateUploadEndpoint),
          net::CertificateReportSender::DO_NOT_SEND_COOKIES)) {}

ChromeFraudulentCertificateReporter::ChromeFraudulentCertificateReporter(
    scoped_ptr<chrome_browser_net::CertificateErrorReporter>
        certificate_reporter)
    : certificate_reporter_(certificate_reporter.Pass()) {
}

ChromeFraudulentCertificateReporter::~ChromeFraudulentCertificateReporter() {
}

void ChromeFraudulentCertificateReporter::SendReport(
    const std::string& hostname,
    const net::SSLInfo& ssl_info) {
  // Do silent/automatic reporting ONLY for Google properties. For other
  // domains (when that is supported), Chrome will ask for user permission.
  if (!net::TransportSecurityState::IsGooglePinnedProperty(hostname))
    return;

  CertificateErrorReport report(hostname, ssl_info);
  std::string serialized_report;
  if (!report.Serialize(&serialized_report)) {
    LOG(ERROR) << "Failed to serialize pinning violation report.";
    return;
  }

  certificate_reporter_->SendPinningViolationReport(serialized_report);
}
