// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_service.h"

#include <ctype.h>
#include <string>

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/variations/metrics_util.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
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

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id) {}
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
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        testing_local_state_(TestingBrowserProcess::GetGlobal()) {
#if defined(OS_CHROMEOS)
    chromeos::FakeDBusThreadManager* fake_dbus_thread_manager =
        new chromeos::FakeDBusThreadManager;
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<chromeos::BluetoothAdapterClient>(
            new chromeos::FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<chromeos::BluetoothDeviceClient>(
            new chromeos::FakeBluetoothDeviceClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<chromeos::BluetoothInputClient>(
            new chromeos::FakeBluetoothInputClient));
    chromeos::DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);
#endif  // OS_CHROMEOS
  }

  virtual ~MetricsServiceTest() {
#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif  // OS_CHROMEOS
    MetricsService::SetExecutionPhase(MetricsService::UNINITIALIZED_PHASE);
  }

  PrefService* GetLocalState() {
    return testing_local_state_.Get();
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
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
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

// Crashes on at least Mac and Linux.  http://crbug.com/320433
TEST_F(MetricsServiceTest, DISABLED_RegisterSyntheticTrial) {
  MetricsService service;

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup trial1(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group1"),
                             base::TimeTicks::Now());
  service.RegisterSyntheticFieldTrial(trial1);

  SyntheticTrialGroup trial2(metrics::HashName("TestTrial2"),
                             metrics::HashName("Group2"),
                             base::TimeTicks::Now());
  service.RegisterSyntheticFieldTrial(trial2);

  service.log_manager_.BeginLoggingWithLog(new MetricsLog("clientID", 1),
                                           MetricsLog::INITIAL_LOG);

  std::vector<chrome_variations::ActiveGroupId> synthetic_trials;
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(2U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group1"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Change the group for the first trial after the log started.
  SyntheticTrialGroup trial3(metrics::HashName("TestTrial1"),
                             metrics::HashName("Group2"),
                             base::TimeTicks::Now());
  service.RegisterSyntheticFieldTrial(trial3);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Add a new trial after the log started and confirm that it doesn't show up.
  SyntheticTrialGroup trial4(metrics::HashName("TestTrial3"),
                             metrics::HashName("Group3"),
                             base::TimeTicks::Now());
  service.RegisterSyntheticFieldTrial(trial4);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(1U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));

  // Start a new log.
  service.log_manager_.FinishCurrentLog();
  service.log_manager_.BeginLoggingWithLog(new MetricsLog("clientID", 1),
                                           MetricsLog::ONGOING_LOG);
  service.GetCurrentSyntheticFieldTrials(&synthetic_trials);
  EXPECT_EQ(3U, synthetic_trials.size());
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial1", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial2", "Group2"));
  EXPECT_TRUE(HasSyntheticTrial(synthetic_trials, "TestTrial3", "Group3"));
  service.log_manager_.FinishCurrentLog();
}
