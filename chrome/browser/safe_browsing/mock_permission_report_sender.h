// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_MOCK_PERMISSION_REPORT_SENDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_MOCK_PERMISSION_REPORT_SENDER_H_

#include "net/url_request/report_sender.h"

namespace safe_browsing {

// A mock ReportSender that keeps track of the last report sent and the number
// of reports sent.
class MockPermissionReportSender : public net::ReportSender {
 public:
  MockPermissionReportSender();

  ~MockPermissionReportSender() override;

  void Send(const GURL& report_uri, const std::string& report) override;

  const GURL& latest_report_uri();

  const std::string& latest_report();

  int GetAndResetNumberOfReportsSent();

 private:
  GURL latest_report_uri_;
  std::string latest_report_;
  int number_of_reports_;

  DISALLOW_COPY_AND_ASSIGN(MockPermissionReportSender);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_MOCK_PERMISSION_REPORT_SENDER_H_
