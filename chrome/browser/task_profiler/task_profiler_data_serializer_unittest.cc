// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void ExpectSerialization(
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    base::ProcessId process_id,
    metrics::ProfilerEventProto::TrackedObject::ProcessType process_type,
    const std::string& expected_json) {
  base::DictionaryValue serialized_value;
  task_profiler::TaskProfilerDataSerializer::ToValue(
      process_data_phase, process_id, process_type, &serialized_value);

  std::string serialized_json;
  base::JSONWriter::Write(serialized_value, &serialized_json);

  EXPECT_EQ(expected_json, serialized_json);
}

}  // anonymous namespace

// Tests the JSON serialization format for profiled process data.
TEST(TaskProfilerDataSerializerTest, SerializeProcessDataToJson) {
  {
    // Empty data.
    tracked_objects::ProcessDataPhaseSnapshot process_data_phase;
    ExpectSerialization(process_data_phase, 239,
                        metrics::ProfilerEventProto::TrackedObject::BROWSER,
                        "{"
                        "\"list\":["
                        "],"
                        "\"process_id\":239,"
                        "\"process_type\":\"Browser\""
                        "}");
  }

  {
    // Non-empty data.
    tracked_objects::ProcessDataPhaseSnapshot process_data_phase;

    tracked_objects::BirthOnThreadSnapshot parent;
    parent.location.file_name = "path/to/foo.cc";
    parent.location.function_name = "WhizBang";
    parent.location.line_number = 101;
    parent.sanitized_thread_name = "CrBrowserMain";

    tracked_objects::BirthOnThreadSnapshot child;
    child.location.file_name = "path/to/bar.cc";
    child.location.function_name = "FizzBoom";
    child.location.line_number = 433;
    child.sanitized_thread_name = "Chrome_IOThread";

    // Add a snapshot.
    process_data_phase.tasks.push_back(tracked_objects::TaskSnapshot());
    process_data_phase.tasks.back().birth = parent;
    process_data_phase.tasks.back().death_data.count = 37;
    process_data_phase.tasks.back().death_data.run_duration_max = 5;
    process_data_phase.tasks.back().death_data.run_duration_sample = 3;
    process_data_phase.tasks.back().death_data.run_duration_sum = 17;
    process_data_phase.tasks.back().death_data.queue_duration_max = 53;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 13;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 79;
    process_data_phase.tasks.back().death_data.alloc_ops = 127;
    process_data_phase.tasks.back().death_data.free_ops = 120;
    process_data_phase.tasks.back().death_data.allocated_bytes = 2013;
    process_data_phase.tasks.back().death_data.freed_bytes = 1092;
    process_data_phase.tasks.back().death_data.alloc_overhead_bytes = 201;
    process_data_phase.tasks.back().death_data.max_allocated_bytes = 1500;
    process_data_phase.tasks.back().death_sanitized_thread_name =
        "WorkerPool/-1340960768";

    // Add a second snapshot.
    process_data_phase.tasks.push_back(tracked_objects::TaskSnapshot());
    process_data_phase.tasks.back().birth = child;
    process_data_phase.tasks.back().death_data.count = 41;
    process_data_phase.tasks.back().death_data.run_duration_max = 205;
    process_data_phase.tasks.back().death_data.run_duration_sample = 203;
    process_data_phase.tasks.back().death_data.run_duration_sum = 2017;
    process_data_phase.tasks.back().death_data.queue_duration_max = 2053;
    process_data_phase.tasks.back().death_data.queue_duration_sample = 2013;
    process_data_phase.tasks.back().death_data.queue_duration_sum = 2079;
    process_data_phase.tasks.back().death_data.alloc_ops = 1207;
    process_data_phase.tasks.back().death_data.free_ops = 1200;
    process_data_phase.tasks.back().death_data.allocated_bytes = 20013;
    process_data_phase.tasks.back().death_data.freed_bytes = 10092;
    process_data_phase.tasks.back().death_data.alloc_overhead_bytes = 2001;
    process_data_phase.tasks.back().death_data.max_allocated_bytes = 15000;
    process_data_phase.tasks.back().death_sanitized_thread_name =
        "PAC thread #3";

    ExpectSerialization(process_data_phase, 239,
                        metrics::ProfilerEventProto::TrackedObject::RENDERER,
                        "{"
                        "\"list\":[{"
                        "\"birth_location\":{"
                        "\"file_name\":\"path/to/foo.cc\","
                        "\"function_name\":\"WhizBang\","
                        "\"line_number\":101"
                        "},"
                        "\"birth_thread\":\"CrBrowserMain\","
                        "\"death_data\":{"
                        "\"alloc_ops\":127,"
                        "\"alloc_overhead_bytes\":201,"
                        "\"allocated_bytes\":2013,"
                        "\"count\":37,"
                        "\"free_ops\":120,"
                        "\"freed_bytes\":1092,"
                        "\"max_allocated_bytes\":1500,"
                        "\"queue_ms\":79,"
                        "\"queue_ms_max\":53,"
                        "\"queue_ms_sample\":13,"
                        "\"run_ms\":17,"
                        "\"run_ms_max\":5,"
                        "\"run_ms_sample\":3"
                        "},"
                        "\"death_thread\":\"WorkerPool/-1340960768\""
                        "},{"
                        "\"birth_location\":{"
                        "\"file_name\":\"path/to/bar.cc\","
                        "\"function_name\":\"FizzBoom\","
                        "\"line_number\":433"
                        "},"
                        "\"birth_thread\":\"Chrome_IOThread\","
                        "\"death_data\":{"
                        "\"alloc_ops\":1207,"
                        "\"alloc_overhead_bytes\":2001,"
                        "\"allocated_bytes\":20013,"
                        "\"count\":41,"
                        "\"free_ops\":1200,"
                        "\"freed_bytes\":10092,"
                        "\"max_allocated_bytes\":15000,"
                        "\"queue_ms\":2079,"
                        "\"queue_ms_max\":2053,"
                        "\"queue_ms_sample\":2013,"
                        "\"run_ms\":2017,"
                        "\"run_ms_max\":205,"
                        "\"run_ms_sample\":203"
                        "},"
                        "\"death_thread\":\"PAC thread #3\""
                        "}],"
                        "\"process_id\":239,"
                        "\"process_type\":\"Tab\""
                        "}");
  }
}
