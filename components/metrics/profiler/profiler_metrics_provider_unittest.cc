// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/profiler/profiler_metrics_provider.h"

#include "base/tracked_objects.h"
#include "components/metrics/metrics_hashes.h"
#include "content/public/common/process_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracked_objects::ProcessDataSnapshot;
using tracked_objects::TaskSnapshot;

namespace metrics {

TEST(ProfilerMetricsProviderTest, RecordData) {
  // WARNING: If you broke the below check, you've modified how
  // HashMetricName works. Please also modify all server-side code that
  // relies on the existing way of hashing.
  EXPECT_EQ(GG_UINT64_C(1518842999910132863), HashMetricName("birth_thread*"));

  ProfilerMetricsProvider profiler_metrics_provider;

  {
    // Add data from the browser process.
    ProcessDataSnapshot process_data;
    process_data.process_id = 177;
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "a/b/file.h";
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
    process_data.tasks.back().birth.location.file_name = "c\\d\\file2";
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

    profiler_metrics_provider.RecordProfilerData(process_data,
                                                 content::PROCESS_TYPE_BROWSER);
  }

  {
    // Add data from a renderer process.
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
    process_data.tasks.push_back(TaskSnapshot());
    process_data.tasks.back().birth.location.file_name = "";
    process_data.tasks.back().birth.location.function_name = "";
    process_data.tasks.back().birth.location.line_number = 7332;
    process_data.tasks.back().birth.thread_name = "";
    process_data.tasks.back().death_data.count = 138;
    process_data.tasks.back().death_data.run_duration_sum = 132;
    process_data.tasks.back().death_data.run_duration_max = 118;
    process_data.tasks.back().death_data.run_duration_sample = 114;
    process_data.tasks.back().death_data.queue_duration_sum = 109;
    process_data.tasks.back().death_data.queue_duration_max = 106;
    process_data.tasks.back().death_data.queue_duration_sample = 104;
    process_data.tasks.back().death_thread_name = "";

    profiler_metrics_provider.RecordProfilerData(
        process_data, content::PROCESS_TYPE_RENDERER);

    // Capture the data and verify that it is as expected.
    ChromeUserMetricsExtension uma_proto;
    profiler_metrics_provider.ProvideGeneralMetrics(&uma_proto);

    ASSERT_EQ(1, uma_proto.profiler_event_size());
    EXPECT_EQ(ProfilerEventProto::STARTUP_PROFILE,
              uma_proto.profiler_event(0).profile_type());
    EXPECT_EQ(ProfilerEventProto::WALL_CLOCK_TIME,
              uma_proto.profiler_event(0).time_source());
    ASSERT_EQ(4, uma_proto.profiler_event(0).tracked_object_size());

    const ProfilerEventProto::TrackedObject* tracked_object =
        &uma_proto.profiler_event(0).tracked_object(0);
    EXPECT_EQ(HashMetricName("file.h"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(HashMetricName("function"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1337, tracked_object->source_line_number());
    EXPECT_EQ(HashMetricName("birth_thread"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(37, tracked_object->exec_count());
    EXPECT_EQ(31, tracked_object->exec_time_total());
    EXPECT_EQ(13, tracked_object->exec_time_sampled());
    EXPECT_EQ(8, tracked_object->queue_time_total());
    EXPECT_EQ(3, tracked_object->queue_time_sampled());
    EXPECT_EQ(HashMetricName("Still_Alive"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());

    tracked_object = &uma_proto.profiler_event(0).tracked_object(1);
    EXPECT_EQ(HashMetricName("file2"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(HashMetricName("function2"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(1773, tracked_object->source_line_number());
    EXPECT_EQ(HashMetricName("birth_thread*"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(19, tracked_object->exec_count());
    EXPECT_EQ(23, tracked_object->exec_time_total());
    EXPECT_EQ(7, tracked_object->exec_time_sampled());
    EXPECT_EQ(0, tracked_object->queue_time_total());
    EXPECT_EQ(0, tracked_object->queue_time_sampled());
    EXPECT_EQ(HashMetricName("death_thread"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::BROWSER,
              tracked_object->process_type());

    tracked_object = &uma_proto.profiler_event(0).tracked_object(2);
    EXPECT_EQ(HashMetricName("file3"),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(HashMetricName("function3"),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7331, tracked_object->source_line_number());
    EXPECT_EQ(HashMetricName("birth_thread*"),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(137, tracked_object->exec_count());
    EXPECT_EQ(131, tracked_object->exec_time_total());
    EXPECT_EQ(113, tracked_object->exec_time_sampled());
    EXPECT_EQ(108, tracked_object->queue_time_total());
    EXPECT_EQ(103, tracked_object->queue_time_sampled());
    EXPECT_EQ(HashMetricName("death_thread*"),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(1177U, tracked_object->process_id());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());

    tracked_object = &uma_proto.profiler_event(0).tracked_object(3);
    EXPECT_EQ(HashMetricName(""),
              tracked_object->source_file_name_hash());
    EXPECT_EQ(HashMetricName(""),
              tracked_object->source_function_name_hash());
    EXPECT_EQ(7332, tracked_object->source_line_number());
    EXPECT_EQ(HashMetricName(""),
              tracked_object->birth_thread_name_hash());
    EXPECT_EQ(138, tracked_object->exec_count());
    EXPECT_EQ(132, tracked_object->exec_time_total());
    EXPECT_EQ(114, tracked_object->exec_time_sampled());
    EXPECT_EQ(109, tracked_object->queue_time_total());
    EXPECT_EQ(104, tracked_object->queue_time_sampled());
    EXPECT_EQ(HashMetricName(""),
              tracked_object->exec_thread_name_hash());
    EXPECT_EQ(ProfilerEventProto::TrackedObject::RENDERER,
              tracked_object->process_type());
  }
}

}  // namespace metrics
