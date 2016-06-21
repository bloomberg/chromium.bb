// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"

#include "base/memory/ptr_util.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "content/public/browser/permission_type.h"
#include "net/url_request/report_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::PermissionType;

namespace safe_browsing {

namespace {
// URL to upload permission action reports.
const char kPermissionActionReportingUploadUrl[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "permission-action";

const char kDummyOrigin[] = "http://example.test/";
const PermissionType kDummyPermission = PermissionType::GEOLOCATION;
const PermissionAction kDummyAction = GRANTED;
const PermissionReport::PermissionType kDummyPermissionReportPermission =
    PermissionReport::GEOLOCATION;
const PermissionReport::Action kDummyPermissionReportAction =
    PermissionReport::GRANTED;

// A mock ReportSender that keeps track of the last report sent.
class MockReportSender : public net::ReportSender {
 public:
  MockReportSender() : net::ReportSender(nullptr, DO_NOT_SEND_COOKIES) {}
  ~MockReportSender() override {}

  void Send(const GURL& report_uri, const std::string& report) override {
    latest_report_uri_ = report_uri;
    latest_report_ = report;
  }

  const GURL& latest_report_uri() { return latest_report_uri_; }

  const std::string& latest_report() { return latest_report_; }

 private:
  GURL latest_report_uri_;
  std::string latest_report_;

  DISALLOW_COPY_AND_ASSIGN(MockReportSender);
};

}  // namespace

class PermissionReporterTest : public ::testing::Test {
 protected:
  PermissionReporterTest()
      : mock_report_sender_(new MockReportSender()),
        permission_reporter_(
            new PermissionReporter(base::WrapUnique(mock_report_sender_))) {}

  // Owned by |permission_reporter_|.
  MockReportSender* mock_report_sender_;

  std::unique_ptr<PermissionReporter> permission_reporter_;
};

// Test that PermissionReporter::SendReport sends a serialized report string to
// SafeBrowsing CSD servers.
TEST_F(PermissionReporterTest, SendReport) {
  permission_reporter_->SendReport(GURL(kDummyOrigin), kDummyPermission,
                                   kDummyAction);

  PermissionReport permission_report;
  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender_->latest_report()));
  EXPECT_EQ(kDummyPermissionReportPermission, permission_report.permission());
  EXPECT_EQ(kDummyPermissionReportAction, permission_report.action());
  EXPECT_EQ(kDummyOrigin, permission_report.origin());

  EXPECT_EQ(GURL(kPermissionActionReportingUploadUrl),
            mock_report_sender_->latest_report_uri());
}

}  // namespace safe_browsing
