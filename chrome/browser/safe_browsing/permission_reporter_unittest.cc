// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/safe_browsing/mock_permission_report_sender.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "components/variations/active_field_trials.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

// URL to upload permission action reports.
const char kPermissionActionReportingUploadUrl[] =
    "https://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "chrome-permissions";

const int kMaximumReportsPerOriginPerPermissionPerMinute = 5;

const char kDummyOriginOne[] = "http://example.test/";
const char kDummyOriginTwo[] = "http://example2.test/";
const ContentSettingsType kDummyPermissionOne =
    CONTENT_SETTINGS_TYPE_GEOLOCATION;
const ContentSettingsType kDummyPermissionTwo =
    CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
const PermissionAction kDummyAction = PermissionAction::GRANTED;
const PermissionSourceUI kDummySourceUI = PermissionSourceUI::PROMPT;
const PermissionRequestGestureType kDummyGestureType =
    PermissionRequestGestureType::GESTURE;
const PermissionPersistDecision kDummyPersistDecision =
    PermissionPersistDecision::PERSISTED;
const int kDummyNumPriorDismissals = 10;
const int kDummyNumPriorIgnores = 12;

const char kDummyTrialOne[] = "trial one";
const char kDummyGroupOne[] = "group one";
const char kDummyTrialTwo[] = "trial two";
const char kDummyGroupTwo[] = "group two";

constexpr char kFeatureOnByDefaultName[] = "OnByDefault";
struct base::Feature kFeatureOnByDefault {
  kFeatureOnByDefaultName, base::FEATURE_ENABLED_BY_DEFAULT
};

constexpr char kFeatureOffByDefaultName[] = "OffByDefault";
struct base::Feature kFeatureOffByDefault {
  kFeatureOffByDefaultName, base::FEATURE_DISABLED_BY_DEFAULT
};

PermissionReportInfo BuildDummyReportInfo(const char* requesting_origin,
                                          ContentSettingsType permission) {
  PermissionReportInfo info(GURL(requesting_origin), permission, kDummyAction,
      kDummySourceUI, kDummyGestureType, kDummyPersistDecision,
      kDummyNumPriorDismissals, kDummyNumPriorIgnores);

  return info;
}

PermissionReportInfo BuildDummyReportInfo() {
  return BuildDummyReportInfo(kDummyOriginOne, kDummyPermissionOne);
}

}  // namespace

class PermissionReporterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_report_sender_ = new MockPermissionReportSender;
    clock_ = new base::SimpleTestClock;
    permission_reporter_.reset(new PermissionReporter(
        base::WrapUnique(mock_report_sender_), base::WrapUnique(clock_)));
  }

  // Owned by |permission_reporter_|.
  MockPermissionReportSender* mock_report_sender_;

  // Owned by |permission_reporter_|.
  base::SimpleTestClock* clock_;

  std::unique_ptr<PermissionReporter> permission_reporter_;
};

// Test that PermissionReporter::SendReport sends a serialized report string to
// SafeBrowsing CSD servers.
TEST_F(PermissionReporterTest, SendReport) {
  permission_reporter_->SendReport(BuildDummyReportInfo());

  PermissionReport permission_report;
  ASSERT_TRUE(
      permission_report.ParseFromString(mock_report_sender_->latest_report()));
  EXPECT_EQ(PermissionReport::GEOLOCATION, permission_report.permission());
  EXPECT_EQ(PermissionReport::GRANTED, permission_report.action());
  EXPECT_EQ(PermissionReport::PROMPT, permission_report.source_ui());
  EXPECT_EQ(PermissionReport::GESTURE, permission_report.gesture());
  EXPECT_EQ(PermissionReport::PERSISTED, permission_report.persisted());
  EXPECT_EQ(kDummyOriginOne, permission_report.origin());
  EXPECT_EQ(kDummyNumPriorDismissals,
            permission_report.num_prior_dismissals());
  EXPECT_EQ(kDummyNumPriorIgnores, permission_report.num_prior_ignores());
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
  EXPECT_EQ("application/octet-stream",
            mock_report_sender_->latest_content_type());
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

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));

  // This is necessary to activate both field trials.
  base::FeatureList::IsEnabled(kFeatureOnByDefault);
  base::FeatureList::IsEnabled(kFeatureOffByDefault);

  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial_one->trial_name()));
  EXPECT_TRUE(base::FieldTrialList::IsTrialActive(trial_two->trial_name()));

  permission_reporter_->SendReport(BuildDummyReportInfo());

  EXPECT_EQ("application/octet-stream",
            mock_report_sender_->latest_content_type());

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

// Test that PermissionReporter::IsReportThresholdExceeded returns false only
// when the number of reports sent in the last one minute per origin per
// permission is under a threshold.
TEST_F(PermissionReporterTest, IsReportThresholdExceeded) {
  EXPECT_EQ(0, mock_report_sender_->GetAndResetNumberOfReportsSent());

  int reports_to_send = kMaximumReportsPerOriginPerPermissionPerMinute;
  while (reports_to_send--)
    permission_reporter_->SendReport(BuildDummyReportInfo());
  EXPECT_EQ(5, mock_report_sender_->GetAndResetNumberOfReportsSent());

  permission_reporter_->SendReport(BuildDummyReportInfo());
  EXPECT_EQ(0, mock_report_sender_->GetAndResetNumberOfReportsSent());

  permission_reporter_->SendReport(
      BuildDummyReportInfo(kDummyOriginOne, kDummyPermissionTwo));
  EXPECT_EQ(1, mock_report_sender_->GetAndResetNumberOfReportsSent());

  permission_reporter_->SendReport(
      BuildDummyReportInfo(kDummyOriginTwo, kDummyPermissionOne));
  EXPECT_EQ(1, mock_report_sender_->GetAndResetNumberOfReportsSent());

  clock_->Advance(base::TimeDelta::FromMinutes(1));
  permission_reporter_->SendReport(BuildDummyReportInfo());

  clock_->Advance(base::TimeDelta::FromMicroseconds(1));
  permission_reporter_->SendReport(BuildDummyReportInfo());
  EXPECT_EQ(1, mock_report_sender_->GetAndResetNumberOfReportsSent());

  clock_->Advance(base::TimeDelta::FromMinutes(1));
  reports_to_send = 12;
  while (reports_to_send--) {
    clock_->Advance(base::TimeDelta::FromSeconds(5));
    permission_reporter_->SendReport(BuildDummyReportInfo());
  }
  EXPECT_EQ(kMaximumReportsPerOriginPerPermissionPerMinute,
            mock_report_sender_->GetAndResetNumberOfReportsSent());
}

}  // namespace safe_browsing
