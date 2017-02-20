// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "base/time/time.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "url/gurl.h"

namespace base {
class Clock;
}  // namespace base

namespace net {
class ReportSender;
class URLRequestContext;
}  // namespace net

namespace safe_browsing {

struct PermissionAndOrigin {
  bool operator==(const PermissionAndOrigin& other) const;

  ContentSettingsType permission;
  GURL origin;
};

struct PermissionAndOriginHash {
  std::size_t operator()(
      const PermissionAndOrigin& permission_and_origin) const;
};

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
  // The permission report includes the origin of the site requesting the
  // permission and other information about the permission action included in
  // |report_info|. The report will be serialized using protobuf defined in
  // //src/chrome/common/safe_browsing/permission_report.proto
  void SendReport(const PermissionReportInfo& report_info);

 private:
  friend class PermissionReporterBrowserTest;
  friend class PermissionReporterTest;

  // Used by tests. This constructor allows tests to have access to the
  // ReportSender and use a test Clock.
  PermissionReporter(std::unique_ptr<net::ReportSender> report_sender,
                     std::unique_ptr<base::Clock> clock);

  // Builds and serializes a permission report with |report_info| included.
  // The serialized report is written into |output|. Returns true if the
  // serialization was successful and false otherwise.
  static bool BuildReport(const PermissionReportInfo& report_info,
                          std::string* output);

  // Returns false if the number of reports sent in the last one minute per
  // origin per permission is under a threshold, otherwise true.
  bool IsReportThresholdExceeded(ContentSettingsType permission,
                                 const GURL& origin);

  std::unique_ptr<net::ReportSender> permission_report_sender_;

  // TODO(stefanocs): This might introduce a memory issue since older entries
  // are not removed until a new report with the corresponding key is added. We
  // should address this issue if that becomes a problem in the future.
  std::unordered_map<PermissionAndOrigin,
                     std::queue<base::Time>,
                     PermissionAndOriginHash>
      report_logs_;

  std::unique_ptr<base::Clock> clock_;

  DISALLOW_COPY_AND_ASSIGN(PermissionReporter);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_PERMISSION_REPORTER_H_
