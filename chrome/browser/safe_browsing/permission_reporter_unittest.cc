// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "components/variations/active_field_trials.h"
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

const char kDummyTrialOne[] = "trial one";
const char kDummyGroupOne[] = "group one";
const char kDummyTrialTwo[] = "trial two";
const char kDummyGroupTwo[] = "group two";

const char kFeatureOnByDefaultName[] = "OnByDefault";
struct base::Feature kFeatureOnByDefault {
  kFeatureOnByDefaultName, base::FEATURE_ENABLED_BY_DEFAULT
};

const char kFeatureOffByDefaultName[] = "OffByDefault";
struct base::Feature kFeatureOffByDefault {
  kFeatureOffByDefaultName, base::FEATURE_DISABLED_BY_DEFAULT
};

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
#if defined(OS_ANDROID)
  EXPECT_EQ(PermissionReport::ANDROID_PLATFORM,
            permission_report.platform_type());
#elif defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS) || \
    defined(OS_LINUX)
  EXPECT_EQ(PermissionReport::DESKTOP_PLATFORM,
            permission_report.platform_type());
#endif

  EXPECT_EQ(GURL(kPermissionActionReportingUploadUrl),
            mock_report_sender_->latest_report_uri());
}

// Test that PermissionReporter::SendReport sends a serialized report string
// with field trials to SafeBrowsing CSD servers.
TEST_F(PermissionReporterTest, SendReportWithFieldTrials) {
  typedef std::set<variations::ActiveGroupId, variations::ActiveGroupIdCompare>
      ActiveGroupIdSet;

  // Add and activate dummy field trials.
  base::FieldTrialList field_trial_list(nullptr);
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  base::FieldTrial* trial_one =
      base::FieldTrialList::CreateFieldTrial(kDummyTrialOne, kDummyGroupOne);
  base::FieldTrial* trial_two =
      base::FieldTrialList::CreateFieldTrial(kDummyTrialTwo, kDummyGroupTwo);

  feature_list->RegisterFieldTrialOverride(
      kFeatureOnByDefaultName, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial_one);
  feature_list->RegisterFieldTrialOverride(
      kFeatureOffByDefaultName, base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial_two);

  base::FeatureList::ClearInstanceForTesting();
  base::FeatureList::SetInstance(std::move(feature_list));

  // This is necessary to activate both field trials.
  base::FeatureList::IsEnabled(kFeatureOnByDefault);
  base::FeatureList::IsEnabled(kFeatureOffByDefault);

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial_one->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial_two->trial_name()));

  permission_reporter_->SendReport(GURL(kDummyOrigin), kDummyPermission,
                                   kDummyAction);

  PermissionReport permission_report;
  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender_->latest_report()));

  variations::ActiveGroupId field_trial_one =
      variations::MakeActiveGroupId(kDummyTrialOne, kDummyGroupOne);
  variations::ActiveGroupId field_trial_two =
      variations::MakeActiveGroupId(kDummyTrialTwo, kDummyGroupTwo);
  ActiveGroupIdSet expected_group_ids = {field_trial_one, field_trial_two};

  EXPECT_EQ(2, permission_report.field_trials().size());
  for (auto field_trial : permission_report.field_trials()) {
    variations::ActiveGroupId group_id = {field_trial.name_id(),
                                          field_trial.group_id()};
    EXPECT_EQ(1U, expected_group_ids.erase(group_id));
  }
  EXPECT_EQ(0U, expected_group_ids.size());
}

}  // namespace safe_browsing
