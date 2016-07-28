// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/mock_permission_report_sender.h"

namespace safe_browsing {

MockPermissionReportSender::MockPermissionReportSender()
    : net::ReportSender(nullptr, DO_NOT_SEND_COOKIES) {
  number_of_reports_ = 0;
}

MockPermissionReportSender::~MockPermissionReportSender() {}

void MockPermissionReportSender::Send(const GURL& report_uri,
                                      const std::string& report) {
  latest_report_uri_ = report_uri;
  latest_report_ = report;
  number_of_reports_++;
}

const GURL& MockPermissionReportSender::latest_report_uri() {
  return latest_report_uri_;
}

const std::string& MockPermissionReportSender::latest_report() {
  return latest_report_;
}

int MockPermissionReportSender::GetAndResetNumberOfReportsSent() {
  int new_reports = number_of_reports_;
  number_of_reports_ = 0;
  return new_reports;
}

}  // namespace safe_browsing
