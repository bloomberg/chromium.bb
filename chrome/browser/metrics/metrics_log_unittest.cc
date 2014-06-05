// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <string>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/port.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_hashes.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/proto/profiler_event.pb.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/variations/active_field_trials.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif  // defined(OS_CHROMEOS)

using base::TimeDelta;

namespace {

const char kClientId[] = "bogus client ID";
const int64 kInstallDate = 1373051956;
const int64 kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
const int64 kEnabledDate = 1373001211;
const int64 kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.
const int kSessionId = 127;
const variations::ActiveGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};
const variations::ActiveGroupId kSyntheticTrials[] = {
  {55, 15},
  {66, 16}
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id,
                 int session_id,
                 LogType log_type,
                 metrics::MetricsServiceClient* client,
                 TestingPrefServiceSimple* prefs)
      : MetricsLog(client_id, session_id, log_type, client, prefs),
        prefs_(prefs) {
    InitPrefs();
  }

  virtual ~TestMetricsLog() {}

  const metrics::ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  const metrics::SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  void InitPrefs() {
    prefs_->SetString(metrics::prefs::kMetricsReportingEnabledTimestamp,
                      base::Int64ToString(kEnabledDate));
  }

  virtual void GetFieldTrialIds(
      std::vector<variations::ActiveGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  // Weak pointer to the PrefsService used by this log.
  TestingPrefServiceSimple* prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() {
    MetricsService::RegisterPrefs(prefs_.registry());
#if defined(OS_CHROMEOS)
    // TODO(blundell): Remove this code once MetricsService no longer creates
    // ChromeOSMetricsProvider. Also remove the #include of login_state.h
    // (http://crbug.com/375776)
    if (!chromeos::LoginState::IsInitialized())
      chromeos::LoginState::Initialize();
#endif  // defined(OS_CHROMEOS)
  }

  virtual ~MetricsLogTest() {
#if defined(OS_CHROMEOS)
    // TODO(blundell): Remove this code once MetricsService no longer creates
    // ChromeOSMetricsProvider.
    chromeos::LoginState::Shutdown();
#endif  // defined(OS_CHROMEOS)
  }

 protected:
  // Check that the values in |system_values| correspond to the test data
  // defined at the top of this file.
  void CheckSystemProfile(const metrics::SystemProfileProto& system_profile) {
    EXPECT_EQ(kInstallDateExpected, system_profile.install_date());
    EXPECT_EQ(kEnabledDateExpected, system_profile.uma_enabled_date());

    ASSERT_EQ(arraysize(kFieldTrialIds) + arraysize(kSyntheticTrials),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }
    // Verify the right data is present for the synthetic trials.
    for (size_t i = 0; i < arraysize(kSyntheticTrials); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i + arraysize(kFieldTrialIds));
      EXPECT_EQ(kSyntheticTrials[i].name, field_trial.name_id());
      EXPECT_EQ(kSyntheticTrials[i].group, field_trial.group_id());
    }

    EXPECT_EQ(metrics::TestMetricsServiceClient::kBrandForTesting,
              system_profile.brand_code());

    const metrics::SystemProfileProto::Hardware& hardware =
        system_profile.hardware();

    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(MetricsLogTest);
};

TEST_F(MetricsLogTest, RecordEnvironment) {
  metrics::TestMetricsServiceClient client;
  client.set_install_date(kInstallDate);
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);

  std::vector<variations::ActiveGroupId> synthetic_trials;
  // Add two synthetic trials.
  synthetic_trials.push_back(kSyntheticTrials[0]);
  synthetic_trials.push_back(kSyntheticTrials[1]);

  log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                        synthetic_trials);
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());

  // Check that the system profile has also been written to prefs.
  const std::string base64_system_profile =
      prefs_.GetString(metrics::prefs::kStabilitySavedSystemProfile);
  EXPECT_FALSE(base64_system_profile.empty());
  std::string serialied_system_profile;
  EXPECT_TRUE(base::Base64Decode(base64_system_profile,
                                 &serialied_system_profile));
  metrics::SystemProfileProto decoded_system_profile;
  EXPECT_TRUE(decoded_system_profile.ParseFromString(serialied_system_profile));
  CheckSystemProfile(decoded_system_profile);
}

TEST_F(MetricsLogTest, LoadSavedEnvironmentFromPrefs) {
  const char* kSystemProfilePref = metrics::prefs::kStabilitySavedSystemProfile;
  const char* kSystemProfileHashPref =
      metrics::prefs::kStabilitySavedSystemProfileHash;

  metrics::TestMetricsServiceClient client;
  client.set_install_date(kInstallDate);

  // The pref value is empty, so loading it from prefs should fail.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
  }

  // Do a RecordEnvironment() call and check whether the pref is recorded.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
    log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                          std::vector<variations::ActiveGroupId>());
    EXPECT_FALSE(prefs_.GetString(kSystemProfilePref).empty());
    EXPECT_FALSE(prefs_.GetString(kSystemProfileHashPref).empty());
  }

  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
    EXPECT_TRUE(log.LoadSavedEnvironmentFromPrefs());
    // Check some values in the system profile.
    EXPECT_EQ(kInstallDateExpected, log.system_profile().install_date());
    EXPECT_EQ(kEnabledDateExpected, log.system_profile().uma_enabled_date());
    // Ensure that the call cleared the prefs.
    EXPECT_TRUE(prefs_.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs_.GetString(kSystemProfileHashPref).empty());
  }

  // Ensure that a non-matching hash results in the pref being invalid.
  {
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
    // Call RecordEnvironment() to record the pref again.
    log.RecordEnvironment(std::vector<metrics::MetricsProvider*>(),
                          std::vector<variations::ActiveGroupId>());
  }

  {
    // Set the hash to a bad value.
    prefs_.SetString(kSystemProfileHashPref, "deadbeef");
    TestMetricsLog log(
        kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
    EXPECT_FALSE(log.LoadSavedEnvironmentFromPrefs());
    // Ensure that the prefs are cleared, even if the call failed.
    EXPECT_TRUE(prefs_.GetString(kSystemProfilePref).empty());
    EXPECT_TRUE(prefs_.GetString(kSystemProfileHashPref).empty());
  }
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(kClientId,
                     kSessionId,
                     MetricsLog::INITIAL_STABILITY_LOG,
                     &client,
                     &prefs_);
  std::vector<metrics::MetricsProvider*> metrics_providers;
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>());
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_TRUE(stability.has_incomplete_shutdown_count());
  EXPECT_TRUE(stability.has_breakpad_registration_success_count());
  EXPECT_TRUE(stability.has_breakpad_registration_failure_count());
  EXPECT_TRUE(stability.has_debugger_present_count());
  EXPECT_TRUE(stability.has_debugger_not_present_count());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  std::vector<metrics::MetricsProvider*> metrics_providers;
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>());
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());
  const metrics::SystemProfileProto_Stability& stability =
      log.system_profile().stability();
  // Required metrics:
  EXPECT_TRUE(stability.has_launch_count());
  EXPECT_TRUE(stability.has_crash_count());
  // Initial log metrics:
  EXPECT_FALSE(stability.has_incomplete_shutdown_count());
  EXPECT_FALSE(stability.has_breakpad_registration_success_count());
  EXPECT_FALSE(stability.has_breakpad_registration_failure_count());
  EXPECT_FALSE(stability.has_debugger_present_count());
  EXPECT_FALSE(stability.has_debugger_not_present_count());
}

TEST_F(MetricsLogTest, ChromeChannelWrittenToProtobuf) {
  metrics::TestMetricsServiceClient client;
  TestMetricsLog log(
      "user@test.com", kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  EXPECT_TRUE(log.uma_proto().system_profile().has_channel());
}
