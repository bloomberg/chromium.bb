// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_service.h"

#include <ctype.h>
#include <string>

#include "base/command_line.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/variations/metrics_util.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/metrics/metrics_log_chromeos.h"
#endif  // OS_CHROMEOS

namespace {

class TestMetricsService : public MetricsService {
 public:
  TestMetricsService() {}
  virtual ~TestMetricsService() {}

  MetricsLogManager* log_manager() {
    return &log_manager_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMetricsService);
};

#if defined(OS_CHROMEOS)
class TestMetricsLogChromeOS : public MetricsLogChromeOS {
 public:
  explicit TestMetricsLogChromeOS(
      metrics::ChromeUserMetricsExtension* uma_proto)
      : MetricsLogChromeOS(uma_proto) {
  }

 protected:
  // Don't touch bluetooth information, as it won't be correctly initialized.
  virtual void WriteBluetoothProto() OVERRIDE {
  }
};
#endif  // OS_CHROMEOS

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id, MetricsLog::ONGOING_LOG) {
#if defined(OS_CHROMEOS)
    metrics_log_chromeos_.reset(new TestMetricsLogChromeOS(
        MetricsLog::uma_proto()));
#endif  // OS_CHROMEOS
  }
  virtual ~TestMetricsLog() {}

 private:
  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(1024, 768);
  }

  virtual float GetScreenDeviceScaleFactor() const OVERRIDE {
    return 1.0f;
  }

  virtual int GetScreenCount() const OVERRIDE {
    return 1;
  }

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

class MetricsServiceTest : public testing::Test {
 public:
  MetricsServiceTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual ~MetricsServiceTest() {
    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE);
  }

  PrefService* GetLocalState() {
    return testing_local_state_.Get();
  }

  // Waits until base::TimeTicks::Now() no longer equals |value|. This should
  // take between 1-15ms per the documented resolution of base::TimeTicks.
  void WaitUntilTimeChanges(const base::TimeTicks& value) {
    while (base::TimeTicks::Now() == value) {
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
    }
  }

  // Returns true if there is a synthetic trial in the given vector that matches
  // the given trial name and trial group; returns false otherwise.
  bool HasSyntheticTrial(
      const std::vector<chrome_variations::ActiveGroupId>& synthetic_trials,
      const std::string& trial_name,
      const std::string& trial_group) {
    uint32 trial_name_hash = metrics::HashName(trial_name);
    uint32 trial_group_hash = metrics::HashName(trial_group);
    for (std::vector<chrome_variations::ActiveGroupId>::const_iterator it =
             synthetic_trials.begin();
         it != synthetic_trials.end(); ++it) {
      if ((*it).name == trial_name_hash && (*it).group == trial_group_hash)
        return true;
    }
    return false;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ScopedTestingLocalState testing_local_state_;

  DISALLOW_COPY_AND_ASSIGN(MetricsServiceTest);
};

}  // namespace

// Ensure the ClientId is formatted as expected.
TEST_F(MetricsServiceTest, ClientIdCorrectlyFormatted) {
  std::string clientid = MetricsService::GenerateClientID();
  EXPECT_EQ(36U, clientid.length());

  for (size_t i = 0; i < clientid.length(); ++i) {
    char current = clientid[i];
    if (i == 8 || i == 13 || i == 18 || i == 23)
      EXPECT_EQ('-', current);
    else
      EXPECT_TRUE(isxdigit(current));
  }
}

TEST_F(MetricsServiceTest, IsPluginProcess) {
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PLUGIN));
  EXPECT_TRUE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_PPAPI_PLUGIN));
  EXPECT_FALSE(
      MetricsService::IsPluginProcess(content::PROCESS_TYPE_GPU));
}

TEST_F(MetricsServiceTest, LowEntropySource0NotReset) {
  MetricsService service;

  // Get the low entropy source once, to initialize it.
  service.GetLowEntropySource();

  // Now, set it to 0 and ensure it doesn't get reset.
  service.low_entropy_source_ = 0;
  EXPECT_EQ(0, service.GetLowEntropySource());
  // Call it another time, just to make sure.
  EXPECT_EQ(0, service.GetLowEntropySource());
}

TEST_F(MetricsServiceTest, PermutedEntropyCacheClearedWhenLowEntropyReset) {
  const PrefService::Preference* low_entropy_pref =
      GetLocalState()->FindPreference(prefs::kMetricsLowEntropySource);
  const char* kCachePrefName = prefs::kMetricsPermutedEntropyCache;
  int low_entropy_value = -1;

  // First, generate an initial low entropy source value.
  {
    EXPECT_TRUE(low_entropy_pref->IsDefaultValue());

    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE);
    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_FALSE(low_entropy_pref->IsDefaultValue());
    EXPECT_TRUE(low_entropy_pref->GetValue()->GetAsInteger(&low_entropy_value));
  }

  // Now, set a dummy value in the permuted entropy cache pref and verify that
  // another call to GetLowEntropySource() doesn't clobber it when
  // --reset-variation-state wasn't specified.
  {
    GetLocalState()->SetString(kCachePrefName, "test");

    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE);
    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_EQ("test", GetLocalState()->GetString(kCachePrefName));
    EXPECT_EQ(low_entropy_value,
              GetLocalState()->GetInteger(prefs::kMetricsLowEntropySource));
  }

  // Verify that the cache does get reset if --reset-variations-state is passed.
  {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kResetVariationState);

    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE);
    MetricsService service;
    service.GetLowEntropySource();

    EXPECT_TRUE(GetLocalState()->GetString(kCachePrefName).empty());
  }
}

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCleanShutDown) {
  base::FieldTrialList field_trial_list(NULL);
  base::FieldTrialList::CreateFieldTrial("UMAStability", "SeparateLog");

  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, true);

  TestMetricsService service;
  service.InitializeMetricsRecordingState(MetricsService::REPORTING_ENABLED);
  // No initial stability log should be generated.
  EXPECT_FALSE(service.log_manager()->has_unsent_logs());
  EXPECT_FALSE(service.log_manager()->has_staged_log());
}

TEST_F(MetricsServiceTest, InitialStabilityLogAfterCrash) {
  base::FieldTrialList field_trial_list(NULL);
  base::FieldTrialList::CreateFieldTrial("UMAStability", "SeparateLog");

  GetLocalState()->ClearPref(prefs::kStabilityExitedCleanly);

  // Set up prefs to simulate restarting after a crash.

  // Save an existing system profile to prefs, to correspond to what would be
  // saved from a previous session.
  TestMetricsLog log("client", 1);
  log.RecordEnvironment(std::vector<content::WebPluginInfo>(),
                        GoogleUpdateMetrics(),
                        std::vector<chrome_variations::ActiveGroupId>());

  // Record stability build time and version from previous session, so that
  // stability metrics (including exited cleanly flag) won't be cleared.
  GetLocalState()->SetInt64(prefs::kStabilityStatsBuildTime,
                            MetricsLog::GetBuildTime());
  GetLocalState()->SetString(prefs::kStabilityStatsVersion,
                             MetricsLog::GetVersionString());

  GetLocalState()->SetBoolean(prefs::kStabilityExitedCleanly, false);

  TestMetricsService service;
  service.InitializeMetricsRecordingState(MetricsService::REPORTING_ENABLED);

  // The initial stability log should be generated and persisted in unsent logs.
  MetricsLogManager* log_manager = service.log_manager();
  EXPECT_TRUE(log_manager->has_unsent_logs());
  EXPECT_FALSE(log_manager->has_staged_log());

  // Stage the log and retrieve it.
  log_manager->StageNextLogForUpload();
  EXPECT_TRUE(log_manager->has_staged_log());

  metrics::ChromeUserMetricsExtension uma_log;
  EXPECT_TRUE(uma_log.ParseFromString(log_manager->staged_log_text()));

  EXPECT_TRUE(uma_log.has_client_id());
  EXPECT_TRUE(uma_log.has_session_id());
  EXPECT_TRUE(uma_log.has_system_profile());
  EXPECT_EQ(0, uma_log.user_action_event_size());
  EXPECT_EQ(0, uma_log.omnibox_event_size());
  EXPECT_EQ(0, uma_log.histogram_event_size());
  EXPECT_EQ(0, uma_log.profiler_event_size());
  EXPECT_EQ(0, uma_log.perf_data_size());

  EXPECT_EQ(1, uma_log.system_profile().stability().crash_count());
}

TEST_F(MetricsServiceTest, RegisterSyntheticTrial) {
  MetricsService service;

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group1"));
  service.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2(metrics::HashName("TestTrial2"),
                             metrics::HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial2);
  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  service.log_manager_.BeginLoggingWithLog(
      new MetricsLog("clientID", 1, MetricsLog::INITIAL_STABILITY_LOG));
  // Save the time when the log was started (it's okay for this to be greater
  // than the time recorded by the above call since it's used to ensure the
  // value changes).
  const base::TimeTicks begin_log_time = base::TimeTicks::Now();

  std::vector<chrome_variations::ActiveGroupId> synthetic_trials;
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(begin_log_time);

  // Change the group for the first trial after the log started.
  // TODO(asvitkine): Assumption that this is > than BeginLoggingWithLog() time.
  SyntheticTrialGroup trial3(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group2"));
  service.RegisterSyntheticFieldTrial(trial3);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  SyntheticTrialGroup trial4(metrics::HashName("TestTrial3"),
                             metrics::HashName("Group3"));
  service.RegisterSyntheticFieldTrial(trial4);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Ensure that time has advanced by at least a tick before proceeding.
  WaitUntilTimeChanges(base::TimeTicks::Now());

  // Start a new log and ensure all three trials appear in it.
  service.log_manager_.FinishCurrentLog();
  service.log_manager_.BeginLoggingWithLog(
      new MetricsLog("clientID", 1, MetricsLog::ONGOING_LOG));
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  service.log_manager_.FinishCurrentLog();
}

TEST_F(MetricsServiceTest, MetricsReportingEnabled) {
#if !defined(OS_CHROMEOS)
  GetLocalState()->SetBoolean(prefs::kMetricsReportingEnabled, false);
  EXPECT_FALSE(MetricsServiceHelper::IsMetricsReportingEnabled());
  GetLocalState()->SetBoolean(prefs::kMetricsReportingEnabled, true);
  EXPECT_TRUE(MetricsServiceHelper::IsMetricsReportingEnabled());
  GetLocalState()->ClearPref(prefs::kMetricsReportingEnabled);
  EXPECT_FALSE(MetricsServiceHelper::IsMetricsReportingEnabled());
#else
  // ChromeOS does not register prefs::kMetricsReportingEnabled and uses
  // device settings for metrics reporting.
  EXPECT_FALSE(MetricsServiceHelper::IsMetricsReportingEnabled());
#endif
}

TEST_F(MetricsServiceTest, CrashReportingEnabled) {
#if defined(GOOGLE_CHROME_BUILD)
// ChromeOS has different device settings for crash reporting.
#if !defined(OS_CHROMEOS)
#if defined(OS_ANDROID)
  const char* crash_pref = prefs::kCrashReportingEnabled;
#else
  const char* crash_pref = prefs::kMetricsReportingEnabled;
#endif
  GetLocalState()->SetBoolean(crash_pref, false);
  EXPECT_FALSE(MetricsServiceHelper::IsCrashReportingEnabled());
  GetLocalState()->SetBoolean(crash_pref, true);
  EXPECT_TRUE(MetricsServiceHelper::IsCrashReportingEnabled());
  GetLocalState()->ClearPref(crash_pref);
  EXPECT_FALSE(MetricsServiceHelper::IsCrashReportingEnabled());
#endif  // !defined(OS_CHROMEOS)
#else  // defined(GOOGLE_CHROME_BUILD)
  // Chromium branded browsers never have crash reporting enabled.
  EXPECT_FALSE(MetricsServiceHelper::IsCrashReportingEnabled());
#endif  // defined(GOOGLE_CHROME_BUILD)
}

// Check that setting the kMetricsResetIds pref to true causes the client id to
// be reset. We do not check that the low entropy source is reset because we
// cannot ensure that metrics service won't generate the same id again.
TEST_F(MetricsServiceTest, ResetMetricsIDs) {
  // Set an initial client id in prefs. It should not be possible for the
  // metrics service to generate this id randomly.
  const std::string kInitialClientId = "initial client id";
  GetLocalState()->SetString(prefs::kMetricsClientID, kInitialClientId);

  // Make sure the initial client id isn't reset by the metrics service.
  {
    MetricsService service;
    service.ForceClientIdCreation();
    EXPECT_TRUE(service.metrics_ids_reset_check_performed_);
    EXPECT_EQ(kInitialClientId, service.client_id_);
  }


  // Set the reset pref to cause the IDs to be reset.
  GetLocalState()->SetBoolean(prefs::kMetricsResetIds, true);

  // Cause the actual reset to happen.
  {
    MetricsService service;
    service.ForceClientIdCreation();
    EXPECT_TRUE(service.metrics_ids_reset_check_performed_);
    EXPECT_NE(kInitialClientId, service.client_id_);

    service.GetLowEntropySource();

    EXPECT_FALSE(GetLocalState()->GetBoolean(prefs::kMetricsResetIds));
  }

  std::string new_client_id =
      GetLocalState()->GetString(prefs::kMetricsClientID);

  EXPECT_NE(kInitialClientId, new_client_id);
}
