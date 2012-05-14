// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/port.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/tracked_objects.h"
#include "chrome/browser/metrics/metrics_log.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/metrics/experiments_helper.h"
#include "chrome/common/metrics/proto/profiler_event.pb.h"
#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/test/base/testing_pref_service.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "webkit/plugins/webplugininfo.h"

using base::TimeDelta;
using metrics::ProfilerEventProto;
using tracked_objects::ProcessDataSnapshot;
using tracked_objects::TaskSnapshot;

namespace {

const char kClientId[] = "bogus client ID";
const int kSessionId = 127;
const int kScreenWidth = 1024;
const int kScreenHeight = 768;
const int kScreenCount = 3;
const experiments_helper::SelectedGroupId kFieldTrialIds[] = {
  {37, 43},
  {13, 47},
  {23, 17}
};

class TestMetricsLog : public MetricsLog {
 public:
  TestMetricsLog(const std::string& client_id, int session_id)
      : MetricsLog(client_id, session_id) {
    browser::RegisterLocalState(&prefs_);

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
  virtual std::string GetCurrentTimeString() OVERRIDE {
    return std::string();
  }

  virtual void GetFieldTrialIds(
      std::vector<experiments_helper::SelectedGroupId>* field_trial_ids) const
      OVERRIDE {
    ASSERT_TRUE(field_trial_ids->empty());

    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      field_trial_ids->push_back(kFieldTrialIds[i]);
    }
  }

  virtual gfx::Size GetScreenSize() const OVERRIDE {
    return gfx::Size(kScreenWidth, kScreenHeight);
  }

  virtual int GetScreenCount() const OVERRIDE {
    return kScreenCount;
  }

  TestingPrefService prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestMetricsLog);
};

}  // namespace

class MetricsLogTest : public testing::Test {
 protected:
  void TestRecordEnvironment(bool proto_only) {
    TestMetricsLog log(kClientId, kSessionId);

    std::vector<webkit::WebPluginInfo> plugins;
    GoogleUpdateMetrics google_update_metrics;
    if (proto_only)
      log.RecordEnvironmentProto(plugins, google_update_metrics);
    else
      log.RecordEnvironment(plugins, google_update_metrics, NULL);

    const metrics::SystemProfileProto& system_profile = log.system_profile();
    ASSERT_EQ(arraysize(kFieldTrialIds),
              static_cast<size_t>(system_profile.field_trial_size()));
    for (size_t i = 0; i < arraysize(kFieldTrialIds); ++i) {
      const metrics::SystemProfileProto::FieldTrial& field_trial =
          system_profile.field_trial(i);
      EXPECT_EQ(kFieldTrialIds[i].name, field_trial.name_id());
      EXPECT_EQ(kFieldTrialIds[i].group, field_trial.group_id());
    }

    // TODO(isherman): Verify other data written into the protobuf as a result
    // of this call.
  }
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
    EXPECT_EQ(GG_UINT64_C(13962325592283560029),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(10123486280357988687),
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
    EXPECT_EQ(GG_UINT64_C(55232426147951219),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(2025659946535236365),
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
    EXPECT_EQ(GG_UINT64_C(5081672290546182009),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(GG_UINT64_C(2686523203278102732),
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
TEST_F(MetricsLogTest, ChromeOSStabilityData) {
  TestMetricsLog log(kClientId, kSessionId);

  // Expect 3 warnings about not yet being able to send the
  // Chrome OS stability stats.
  std::vector<webkit::WebPluginInfo> plugins;
  PrefService* prefs = log.GetPrefService();
  log.WriteStabilityElement(plugins, prefs);
  log.CloseLog();

  int size = log.GetEncodedLogSizeXml();
  ASSERT_GT(size, 0);

  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityChildProcessCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityOtherUserCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilityKernelCrashCount));
  EXPECT_EQ(0, prefs->GetInteger(prefs::kStabilitySystemUncleanShutdownCount));

  std::string encoded;
  // Leave room for the NUL terminator.
  bool encoding_result = log.GetEncodedLogXml(
      WriteInto(&encoded, size + 1), size);
  ASSERT_TRUE(encoding_result);

  // Check that we can find childprocesscrashcount, but not
  // any of the ChromeOS ones that we are not emitting until log
  // servers can handle them.
  EXPECT_NE(std::string::npos,
            encoded.find(" childprocesscrashcount=\"10\""));
  EXPECT_EQ(std::string::npos,
            encoded.find(" otherusercrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" kernelcrashcount="));
  EXPECT_EQ(std::string::npos,
            encoded.find(" systemuncleanshutdowns="));
}
#endif  // OS_CHROMEOS
