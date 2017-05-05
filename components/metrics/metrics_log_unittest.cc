// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/base64.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/sample_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "components/metrics/environment_recorder.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/active_field_trials.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if defined(OS_WIN)
#include "base/win/current_module.h"
#endif

namespace metrics {

namespace {

const char kClientId[] = "bogus client ID";
const int64_t kInstallDate = 1373051956;
const int64_t kInstallDateExpected = 1373050800;  // Computed from kInstallDate.
const int64_t kEnabledDate = 1373001211;
const int64_t kEnabledDateExpected = 1373000400;  // Computed from kEnabledDate.
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
                 MetricsServiceClient* client,
                 TestingPrefServiceSimple* prefs)
      : MetricsLog(client_id, session_id, log_type, client, prefs),
        prefs_(prefs) {
    InitPrefs();
  }

  ~TestMetricsLog() override {}

  const ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  const SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  void InitPrefs() {
    prefs_->SetString(prefs::kMetricsReportingEnabledTimestamp,
                      base::Int64ToString(kEnabledDate));
  }

  void GetFieldTrialIds(
      std::vector<variations::ActiveGroupId>* field_trial_ids) const override {
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
    EnvironmentRecorder::RegisterPrefs(prefs_.registry());
    MetricsStateManager::RegisterPrefs(prefs_.registry());
  }

  ~MetricsLogTest() override {}

 protected:
  // Check that the values in |system_values| correspond to the test data
  // defined at the top of this file.
  void CheckSystemProfile(const SystemProfileProto& system_profile) {
    EXPECT_EQ(kInstallDateExpected, system_profile.install_date());
    EXPECT_EQ(kEnabledDateExpected, system_profile.uma_enabled_date());

    ASSERT_EQ(arraysize(kFieldTrialIds) + arraysize(kSyntheticTrials),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }
    // Verify the right data is present for the synthetic trials.
    for (size_t i = 0; i < arraysize(kSyntheticTrials); ++i) {
      const SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i + arraysize(kFieldTrialIds));
      EXPECT_EQ(kSyntheticTrials[i].name, field_trial.name_id());
      EXPECT_EQ(kSyntheticTrials[i].group, field_trial.group_id());
    }

    EXPECT_EQ(TestMetricsServiceClient::kBrandForTesting,
              system_profile.brand_code());

    const SystemProfileProto::Hardware& hardware =
        system_profile.hardware();

    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());
    EXPECT_TRUE(hardware.cpu().has_num_cores());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsLogTest);
};

TEST_F(MetricsLogTest, LogType) {
  TestMetricsServiceClient client;
  TestingPrefServiceSimple prefs;

  MetricsLog log1("id", 0, MetricsLog::ONGOING_LOG, &client, &prefs);
  EXPECT_EQ(MetricsLog::ONGOING_LOG, log1.log_type());

  MetricsLog log2("id", 0, MetricsLog::INITIAL_STABILITY_LOG, &client, &prefs);
  EXPECT_EQ(MetricsLog::INITIAL_STABILITY_LOG, log2.log_type());
}

TEST_F(MetricsLogTest, BasicRecord) {
  TestMetricsServiceClient client;
  client.set_version_string("bogus version");
  TestingPrefServiceSimple prefs;
  MetricsLog log("totally bogus client ID", 137, MetricsLog::ONGOING_LOG,
                 &client, &prefs);
  log.CloseLog();

  std::string encoded;
  log.GetEncodedLog(&encoded);

  // A couple of fields are hard to mock, so these will be copied over directly
  // for the expected output.
  ChromeUserMetricsExtension parsed;
  ASSERT_TRUE(parsed.ParseFromString(encoded));

  ChromeUserMetricsExtension expected;
  expected.set_client_id(5217101509553811875);  // Hashed bogus client ID
  expected.set_session_id(137);

  SystemProfileProto* system_profile = expected.mutable_system_profile();
  system_profile->set_app_version("bogus version");
  system_profile->set_channel(client.GetChannel());
  system_profile->set_application_locale(client.GetApplicationLocale());

#if defined(SYZYASAN)
  system_profile->set_is_asan_build(true);
#endif
  metrics::SystemProfileProto::Hardware* hardware =
      system_profile->mutable_hardware();
#if !defined(OS_IOS)
  hardware->set_cpu_architecture(base::SysInfo::OperatingSystemArchitecture());
#endif
  hardware->set_system_ram_mb(base::SysInfo::AmountOfPhysicalMemoryMB());
  hardware->set_hardware_class(base::SysInfo::HardwareModelName());
#if defined(OS_WIN)
  hardware->set_dll_base(reinterpret_cast<uint64_t>(CURRENT_MODULE()));
#endif

  system_profile->mutable_os()->set_name(base::SysInfo::OperatingSystemName());
  system_profile->mutable_os()->set_version(
      base::SysInfo::OperatingSystemVersion());
#if defined(OS_ANDROID)
  system_profile->mutable_os()->set_fingerprint(
      base::android::BuildInfo::GetInstance()->android_build_fp());
#endif

  // Hard to mock.
  system_profile->set_build_timestamp(
      parsed.system_profile().build_timestamp());

  EXPECT_EQ(expected.SerializeAsString(), encoded);
}

TEST_F(MetricsLogTest, HistogramBucketFields) {
  // Create buckets: 1-5, 5-7, 7-8, 8-9, 9-10, 10-11, 11-12.
  base::BucketRanges ranges(8);
  ranges.set_range(0, 1);
  ranges.set_range(1, 5);
  ranges.set_range(2, 7);
  ranges.set_range(3, 8);
  ranges.set_range(4, 9);
  ranges.set_range(5, 10);
  ranges.set_range(6, 11);
  ranges.set_range(7, 12);

  base::SampleVector samples(1, &ranges);
  samples.Accumulate(3, 1);   // Bucket 1-5.
  samples.Accumulate(6, 1);   // Bucket 5-7.
  samples.Accumulate(8, 1);   // Bucket 8-9. (7-8 skipped)
  samples.Accumulate(10, 1);  // Bucket 10-11. (9-10 skipped)
  samples.Accumulate(11, 1);  // Bucket 11-12.

  TestMetricsServiceClient client;
  TestingPrefServiceSimple prefs;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  log.RecordHistogramDelta("Test", samples);

  const ChromeUserMetricsExtension& uma_proto = log.uma_proto();
  const HistogramEventProto& histogram_proto =
      uma_proto.histogram_event(uma_proto.histogram_event_size() - 1);

  // Buckets with samples: 1-5, 5-7, 8-9, 10-11, 11-12.
  // Should become: 1-/, 5-7, /-9, 10-/, /-12.
  ASSERT_EQ(5, histogram_proto.bucket_size());

  // 1-5 becomes 1-/ (max is same as next min).
  EXPECT_TRUE(histogram_proto.bucket(0).has_min());
  EXPECT_FALSE(histogram_proto.bucket(0).has_max());
  EXPECT_EQ(1, histogram_proto.bucket(0).min());

  // 5-7 stays 5-7 (no optimization possible).
  EXPECT_TRUE(histogram_proto.bucket(1).has_min());
  EXPECT_TRUE(histogram_proto.bucket(1).has_max());
  EXPECT_EQ(5, histogram_proto.bucket(1).min());
  EXPECT_EQ(7, histogram_proto.bucket(1).max());

  // 8-9 becomes /-9 (min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(2).has_min());
  EXPECT_TRUE(histogram_proto.bucket(2).has_max());
  EXPECT_EQ(9, histogram_proto.bucket(2).max());

  // 10-11 becomes 10-/ (both optimizations apply, omit max is prioritized).
  EXPECT_TRUE(histogram_proto.bucket(3).has_min());
  EXPECT_FALSE(histogram_proto.bucket(3).has_max());
  EXPECT_EQ(10, histogram_proto.bucket(3).min());

  // 11-12 becomes /-12 (last record must keep max, min is same as max - 1).
  EXPECT_FALSE(histogram_proto.bucket(4).has_min());
  EXPECT_TRUE(histogram_proto.bucket(4).has_max());
  EXPECT_EQ(12, histogram_proto.bucket(4).max());
}

TEST_F(MetricsLogTest, RecordEnvironment) {
  TestMetricsServiceClient client;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);

  std::vector<variations::ActiveGroupId> synthetic_trials;
  // Add two synthetic trials.
  synthetic_trials.push_back(kSyntheticTrials[0]);
  synthetic_trials.push_back(kSyntheticTrials[1]);

  log.RecordEnvironment(std::vector<std::unique_ptr<MetricsProvider>>(),
                        synthetic_trials, kInstallDate, kEnabledDate);
  // Check that the system profile on the log has the correct values set.
  CheckSystemProfile(log.system_profile());

  // Check that the system profile has also been written to prefs.
  SystemProfileProto decoded_system_profile;
  EnvironmentRecorder recorder(&prefs_);
  EXPECT_TRUE(recorder.LoadEnvironmentFromPrefs(&decoded_system_profile));
  CheckSystemProfile(decoded_system_profile);
}

TEST_F(MetricsLogTest, RecordEnvironmentEnableDefault) {
  TestMetricsServiceClient client;
  TestMetricsLog log_unknown(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client, &prefs_);

  std::vector<variations::ActiveGroupId> synthetic_trials;

  log_unknown.RecordEnvironment(std::vector<std::unique_ptr<MetricsProvider>>(),
                                synthetic_trials, kInstallDate, kEnabledDate);
  EXPECT_FALSE(log_unknown.system_profile().has_uma_default_state());

  client.set_enable_default(EnableMetricsDefault::OPT_IN);
  TestMetricsLog log_opt_in(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                            &client, &prefs_);
  log_opt_in.RecordEnvironment(std::vector<std::unique_ptr<MetricsProvider>>(),
                               synthetic_trials, kInstallDate, kEnabledDate);
  EXPECT_TRUE(log_opt_in.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_IN,
            log_opt_in.system_profile().uma_default_state());

  client.set_enable_default(EnableMetricsDefault::OPT_OUT);
  TestMetricsLog log_opt_out(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client, &prefs_);
  log_opt_out.RecordEnvironment(std::vector<std::unique_ptr<MetricsProvider>>(),
                                synthetic_trials, kInstallDate, kEnabledDate);
  EXPECT_TRUE(log_opt_out.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_OPT_OUT,
            log_opt_out.system_profile().uma_default_state());

  client.set_reporting_is_managed(true);
  TestMetricsLog log_managed(kClientId, kSessionId, MetricsLog::ONGOING_LOG,
                             &client, &prefs_);
  log_managed.RecordEnvironment(std::vector<std::unique_ptr<MetricsProvider>>(),
                                synthetic_trials, kInstallDate, kEnabledDate);
  EXPECT_TRUE(log_managed.system_profile().has_uma_default_state());
  EXPECT_EQ(SystemProfileProto_UmaDefaultState_POLICY_FORCED_ENABLED,
            log_managed.system_profile().uma_default_state());
}

TEST_F(MetricsLogTest, InitialLogStabilityMetrics) {
  TestMetricsServiceClient client;
  TestMetricsLog log(kClientId,
                     kSessionId,
                     MetricsLog::INITIAL_STABILITY_LOG,
                     &client,
                     &prefs_);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  std::vector<std::unique_ptr<MetricsProvider>> metrics_providers;
  metrics_providers.push_back(base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>(), kInstallDate,
                        kEnabledDate);
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());

  // The test provider should have been called upon to provide initial
  // stability and regular stability metrics.
  EXPECT_TRUE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, OngoingLogStabilityMetrics) {
  TestMetricsServiceClient client;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  TestMetricsProvider* test_provider = new TestMetricsProvider();
  std::vector<std::unique_ptr<MetricsProvider>> metrics_providers;
  metrics_providers.push_back(base::WrapUnique<MetricsProvider>(test_provider));
  log.RecordEnvironment(metrics_providers,
                        std::vector<variations::ActiveGroupId>(), kInstallDate,
                        kEnabledDate);
  log.RecordStabilityMetrics(metrics_providers, base::TimeDelta(),
                             base::TimeDelta());

  // The test provider should have been called upon to provide regular but not
  // initial stability metrics.
  EXPECT_FALSE(test_provider->provide_initial_stability_metrics_called());
  EXPECT_TRUE(test_provider->provide_stability_metrics_called());
}

TEST_F(MetricsLogTest, ChromeChannelWrittenToProtobuf) {
  TestMetricsServiceClient client;
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  EXPECT_TRUE(log.uma_proto().system_profile().has_channel());
}

TEST_F(MetricsLogTest, ProductNotSetIfDefault) {
  TestMetricsServiceClient client;
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, client.GetProduct());
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  // Check that the product isn't set, since it's default and also verify the
  // default value is indeed equal to Chrome.
  EXPECT_FALSE(log.uma_proto().has_product());
  EXPECT_EQ(ChromeUserMetricsExtension::CHROME, log.uma_proto().product());
}

TEST_F(MetricsLogTest, ProductSetIfNotDefault) {
  const int32_t kTestProduct = 100;
  EXPECT_NE(ChromeUserMetricsExtension::CHROME, kTestProduct);

  TestMetricsServiceClient client;
  client.set_product(kTestProduct);
  TestMetricsLog log(
      kClientId, kSessionId, MetricsLog::ONGOING_LOG, &client, &prefs_);
  // Check that the product is set to |kTestProduct|.
  EXPECT_TRUE(log.uma_proto().has_product());
  EXPECT_EQ(kTestProduct, log.uma_proto().product());
}

}  // namespace metrics
