// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/port.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/proto/profiler_event.pb.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/variations/entropy_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/process_type.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#endif  // OS_CHROMEOS

using base::TimeDelta;
using metrics::ProfilerEventProto;
using tracked_objects::ProcessDataSnapshot;
using tracked_objects::TaskSnapshot;

namespace {

const char kClientId[] = "bogus client ID";
const int64 kInstallDate = 1373051956;
const int64 kEnabledDate = 1373001211;
const int kSessionId = 127;
const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const float kScreenScaleFactor = 2;
const char kBrandForTesting[] = "brand_for_testing";
const chrome_variations::ActiveGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id),
        brand_for_testing_(kBrandForTesting) {
    chrome::RegisterLocalState(prefs_.registry());

    prefs_.SetInt64(prefs::kInstallDate, kInstallDate);
    prefs_.SetString(prefs::kMetricsClientIDTimestamp,
                     base::Int64ToString(kEnabledDate));
#if defined(OS_CHROMEOS)
    prefs_.SetInteger(prefs::kStabilityChildProcessCrashCount, 10);
    prefs_.SetInteger(prefs::kStabilityOtherUserCrashCount, 11);
    prefs_.SetInteger(prefs::kStabilityKernelCrashCount, 12);
    prefs_.SetInteger(prefs::kStabilitySystemUncleanShutdownCount, 13);
#endif  // OS_CHROMEOS
  }
  virtual ~TestMetricsLog() {}

  virtual PrefService* GetPrefService() OVERRIDE {
    return &prefs_;
  }

  const metrics::ChromeUserMetricsExtension& uma_proto() const {
    return *MetricsLog::uma_proto();
  }

  const metrics::SystemProfileProto& system_profile() const {
    return uma_proto().system_profile();
  }

 private:
  virtual void GetFieldTrialIds(
      std::vector<chrome_variations::ActiveGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(kScreenWidth, kScreenHeight);
  }

  virtual float GetScreenDeviceScaleFactor() const OVERRIDE {
    return kScreenScaleFactor;
  }

  virtual int GetScreenCount() const OVERRIDE {
    return kScreenCount;
  }

  TestingPrefServiceSimple prefs_;

  google_util::BrandForTesting brand_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 public:
  MetricsLogTest() : message_loop_(base::MessageLoop::TYPE_IO) {}

 protected:
  void TestRecordEnvironment(bool proto_only) {
    TestMetricsLog log(kClientId, kSessionId);

    std::vector<content::WebPluginInfo> plugins;
    GoogleUpdateMetrics google_update_metrics;
    if (proto_only)
      log.RecordEnvironmentProto(plugins, google_update_metrics);
    else
      log.RecordEnvironment(plugins, google_update_metrics, base::TimeDelta());

    // Computed from original time of 1373051956.
    EXPECT_EQ(1373050800, log.system_profile().install_date());

    // Computed from original time of 1373001211.
    EXPECT_EQ(1373000400, log.system_profile().uma_enabled_date());

    const metrics::SystemProfileProto& system_profile = log.system_profile();
    ASSERT_EQ(arraysize(kFieldTrialIds),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }

    EXPECT_EQ(kBrandForTesting, system_profile.brand_code());

    const metrics::SystemProfileProto::Hardware& hardware =
        system_profile.hardware();
    EXPECT_EQ(kScreenWidth, hardware.primary_screen_width());
    EXPECT_EQ(kScreenHeight, hardware.primary_screen_height());
    EXPECT_EQ(kScreenScaleFactor, hardware.primary_screen_scale_factor());
    EXPECT_EQ(kScreenCount, hardware.screen_count());

    EXPECT_TRUE(hardware.has_cpu());
    EXPECT_TRUE(hardware.cpu().has_vendor_name());
    EXPECT_TRUE(hardware.cpu().has_signature());

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }

  virtual void SetUp() OVERRIDE {
#if defined(OS_CHROMEOS)
    fake_dbus_thread_manager_ = new chromeos::FakeDBusThreadManager();
    chromeos::DBusThreadManager::InitializeForTesting(
        fake_dbus_thread_manager_);

    // Enable multi-profiles.
    CommandLine::ForCurrentProcess()->AppendSwitch(switches::kMultiProfiles);
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
    base::FieldTrialList::CreateTrialsFromString(
        "ChromeOSUseMultiProfiles/Enable/",
        base::FieldTrialList::ACTIVATE_TRIALS);
#endif  // OS_CHROMEOS
  }

  virtual void TearDown() OVERRIDE {
    // Drain the blocking pool from PostTaskAndReply executed by
    // MetrticsLog.network_observer_.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    content::RunAllPendingInMessageLoop();

#if defined(OS_CHROMEOS)
    chromeos::DBusThreadManager::Shutdown();
#endif  // OS_CHROMEOS
  }

 private:
  // This is necessary because eventually some tests call base::RepeatingTimer
  // functions and a message loop is required for that.
  base::MessageLoop message_loop_;

#if defined(OS_CHROMEOS)
  chromeos::FakeDBusThreadManager* fake_dbus_thread_manager_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
#endif  // OS_CHROMEOS
};

TEST_F(MetricsLogTest, RecordEnvironment) {
  // Test that recording the environment works via both of the public methods
  // RecordEnvironment() and RecordEnvironmentProto().
  TestRecordEnvironment(false);
  TestRecordEnvironment(true);
}

// Test that we properly write profiler data to the log.
TEST_F(MetricsLogTest, RecordProfilerData) {
  TestMetricsLog log(kClientId, kSessionId);
  EXPECT_EQ(0, log.uma_proto().profiler_event_size());

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file";
    process_data.tasks.back().birth.location.function_name = "function";
    process_data.tasks.back().birth.location.line_number = 1337;
    process_data.tasks.back().birth.thread_name = "birth_thread";
    process_data.tasks.back().death_data.count = 37;
    process_data.tasks.back().death_data.run_duration_sum = 31;
    process_data.tasks.back().death_data.run_duration_max = 17;
    process_data.tasks.back().death_data.run_duration_sample = 13;
    process_data.tasks.back().death_data.queue_duration_sum = 8;
    process_data.tasks.back().death_data.queue_duration_max = 5;
    process_data.tasks.back().death_data.queue_duration_sample = 3;
    process_data.tasks.back().death_thread_name = "Still_Alive";
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file2";
    process_data.tasks.back().birth.location.function_name = "function2";
    process_data.tasks.back().birth.location.line_number = 1773;
    process_data.tasks.back().birth.thread_name = "birth_thread2";
    process_data.tasks.back().death_data.count = 19;
    process_data.tasks.back().death_data.run_duration_sum = 23;
    process_data.tasks.back().death_data.run_duration_max = 11;
    process_data.tasks.back().death_data.run_duration_sample = 7;
    process_data.tasks.back().death_data.queue_duration_sum = 0;
    process_data.tasks.back().death_data.queue_duration_max = 0;
    process_data.tasks.back().death_data.queue_duration_sample = 0;
    process_data.tasks.back().death_thread_name = "death_thread";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_BROWSER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());

    ASSERT_EQ(2, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(0);
    EXPECT_EQ(GG_UINT64_C(10123486280357988687),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(13962325592283560029),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1337, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(3400908935414830400),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(37, tracked_object->exec_count());
    EXPECT_EQ(31, tracked_object->exec_time_total());
    EXPECT_EQ(13, tracked_object->exec_time_sampled());
    EXPECT_EQ(8, tracked_object->queue_time_total());
    EXPECT_EQ(3, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(10151977472163283085),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());

    tracked_object = &log.uma_proto().profiler_event(0).tracked_object(1);
    EXPECT_EQ(GG_UINT64_C(2025659946535236365),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(55232426147951219),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1773, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(15727396632046120663),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(19, tracked_object->exec_count());
    EXPECT_EQ(23, tracked_object->exec_time_total());
    EXPECT_EQ(7, tracked_object->exec_time_sampled());
    EXPECT_EQ(0, tracked_object->queue_time_total());
    EXPECT_EQ(0, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(14275151213201158253),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());
  }

  {
    ProcessDataSnapshot process_data;
    process_data.process_id = 1177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "file3";
    process_data.tasks.back().birth.location.function_name = "function3";
    process_data.tasks.back().birth.location.line_number = 7331;
    process_data.tasks.back().birth.thread_name = "birth_thread3";
    process_data.tasks.back().death_data.count = 137;
    process_data.tasks.back().death_data.run_duration_sum = 131;
    process_data.tasks.back().death_data.run_duration_max = 117;
    process_data.tasks.back().death_data.run_duration_sample = 113;
    process_data.tasks.back().death_data.queue_duration_sum = 108;
    process_data.tasks.back().death_data.queue_duration_max = 105;
    process_data.tasks.back().death_data.queue_duration_sample = 103;
    process_data.tasks.back().death_thread_name = "death_thread3";

    log.RecordProfilerData(process_data, content::PROCESS_TYPE_RENDERER);
    ASSERT_EQ(1, log.uma_proto().profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              log.uma_proto().profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              log.uma_proto().profiler_event(0).time_source());
    ASSERT_EQ(3, log.uma_proto().profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &log.uma_proto().profiler_event(0).tracked_object(2);
    EXPECT_EQ(GG_UINT64_C(2686523203278102732),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(5081672290546182009),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7331, tracked_object->source_line_number());
    EXPECT_EQ(GG_UINT64_C(8768512930949373716),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(137, tracked_object->exec_count());
    EXPECT_EQ(131, tracked_object->exec_time_total());
    EXPECT_EQ(113, tracked_object->exec_time_sampled());
    EXPECT_EQ(108, tracked_object->queue_time_total());
    EXPECT_EQ(103, tracked_object->queue_time_sampled());
    EXPECT_EQ(GG_UINT64_C(7246674144371406371),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(1177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());
  }
}

#if defined(OS_CHROMEOS)
TEST_F(MetricsLogTest, MultiProfileUserCount) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);
  user_manager->LoginUser(user3);

  TestMetricsLog log(kClientId, kSessionId);
  std::vector<content::WebPluginInfo> plugins;
  GoogleUpdateMetrics google_update_metrics;
  log.RecordEnvironmentProto(plugins, google_update_metrics);
  EXPECT_EQ(2u, log.system_profile().multi_profile_user_count());
}

TEST_F(MetricsLogTest, MultiProfileCountInvalidated) {
  std::string user1("user1@example.com");
  std::string user2("user2@example.com");
  std::string user3("user3@example.com");

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeUserManager* user_manager = new chromeos::FakeUserManager();
  chromeos::ScopedUserManagerEnabler scoped_enabler(user_manager);
  user_manager->AddKioskAppUser(user1);
  user_manager->AddKioskAppUser(user2);
  user_manager->AddKioskAppUser(user3);

  user_manager->LoginUser(user1);

  TestMetricsLog log(kClientId, kSessionId);
  EXPECT_EQ(1u, log.system_profile().multi_profile_user_count());

  user_manager->LoginUser(user2);
  log.RecordEnvironmentProto(std::vector<content::WebPluginInfo>(),
                             GoogleUpdateMetrics());
  EXPECT_EQ(0u, log.system_profile().multi_profile_user_count());
}
#endif  // OS_CHROMEOS
