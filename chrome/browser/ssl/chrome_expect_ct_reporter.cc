// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/chrome_expect_ct_reporter.h"

#include <string>

#include "net/url_request/certificate_report_sender.h"

ChromeExpectCTReporter::ChromeExpectCTReporter(
    net::URLRequestContext* request_context)
    : report_sender_(new net::CertificateReportSender(
          request_context,
          net::CertificateReportSender::DO_NOT_SEND_COOKIES)) {}

ChromeExpectCTReporter::~ChromeExpectCTReporter() {}

void ChromeExpectCTReporter::OnExpectCTFailed(
    const net::HostPortPair& host_port_pair,
    const GURL& report_uri,
    const net::SSLInfo& ssl_info) {
  // TODO(estark): build and send a report about the policy
  // violation. https://crbug.com/568806
}
