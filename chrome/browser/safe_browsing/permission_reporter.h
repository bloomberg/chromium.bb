// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_

#include <string>

#include "chrome/browser/permissions/permission_uma_util.h"
#include "url/gurl.h"

namespace net {
class ReportSender;
class URLRequestContext;
}  // namespace net

namespace safe_browsing {

// Provides functionality for building and serializing reports about permissions
// to a report collection server.
class PermissionReporter {
 public:
  // Creates a permission reporter that will send permission reports to
  // the SafeBrowsing permission action server, using |request_context| as the
  // context for the reports.
  explicit PermissionReporter(net::URLRequestContext* request_context);

  ~PermissionReporter();

  // Sends a serialized permission report to the report collection server.
  // The permission report includes |origin| as the origin of
  // the site requesting permission, |permission| as the type of permission
  // requested, and |action| as the action taken. The report will be serialized
  // using protobuf defined in
  // //src/chrome/common/safe_browsing/permission_report.proto
  void SendReport(const GURL& origin,
                  content::PermissionType permission,
                  PermissionAction action);

 private:
  friend class PermissionReporterTest;

  // Used by tests. This constructor allows tests to have access to the
  // ReportSender.
  explicit PermissionReporter(std::unique_ptr<net::ReportSender> report_sender);

  // Builds and serializes a permission report with |origin| as the origin of
  // the site requesting permission, |permission| as the type of permission
  // requested, and |action| as the action taken. The serialized report is
  // written into |output|. Returns true if the serialization was successful and
  // false otherwise.
  static bool BuildReport(const GURL& origin,
                          content::PermissionType permission,
                          PermissionAction action,
                          std::string* output);

  std::unique_ptr<net::ReportSender> permission_report_sender_;

  DISALLOW_COPY_AND_ASSIGN(PermissionReporter);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_
