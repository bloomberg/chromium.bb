// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/json/json_writer.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/tracked_objects.h"
#include "base/values.h"
#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"
#include "content/public/common/process_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string GetProcessIdString() {
  return base::IntToString(base::GetCurrentProcId());
}

void ExpectSerialization(
    const tracked_objects::ProcessDataSnapshot& process_data,
    int process_type,
    const std::string& expected_json) {
  base::DictionaryValue serialized_value;
  task_profiler::TaskProfilerDataSerializer::ToValue(
      process_data, process_type, &serialized_value);

  std::string serialized_json;
  base::JSONWriter::Write(&serialized_value, &serialized_json);

  EXPECT_EQ(expected_json, serialized_json);
}

}  // anonymous namespace

// Tests the JSON serialization format for profiled process data.
TEST(TaskProfilerDataSerializerTest, SerializeProcessDataToJson) {
  {
    // Empty data.
    tracked_objects::ProcessDataSnapshot process_data;
    int process_type = content::PROCESS_TYPE_BROWSER;
    ExpectSerialization(process_data, process_type,
                        "{"
                          "\"descendants\":["
                          "],"
                          "\"list\":["
                          "],"
                          "\"process_id\":" + GetProcessIdString() + ","
                          "\"process_type\":\"Browser\""
                        "}");
  }

  {
    // Non-empty data.
    tracked_objects::ProcessDataSnapshot process_data;

    tracked_objects::BirthOnThreadSnapshot parent;
    parent.location.file_name = "path/to/foo.cc";
    parent.location.function_name = "WhizBang";
    parent.location.line_number = 101;
    parent.thread_name = "CrBrowserMain";

    tracked_objects::BirthOnThreadSnapshot child;
    child.location.file_name = "path/to/bar.cc";
    child.location.function_name = "FizzBoom";
    child.location.line_number = 433;
    child.thread_name = "Chrome_IOThread";


    // Add a snapshot.
    process_data.tasks.push_back(tracked_objects::TaskSnapshot());
    process_data.tasks.back().birth = parent;
    process_data.tasks.back().death_data.count = 37;
    process_data.tasks.back().death_data.run_duration_max = 5;
    process_data.tasks.back().death_data.run_duration_sample = 3;
    process_data.tasks.back().death_data.run_duration_sum = 17;
    process_data.tasks.back().death_data.queue_duration_max = 53;
    process_data.tasks.back().death_data.queue_duration_sample = 13;
    process_data.tasks.back().death_data.queue_duration_sum = 79;
    process_data.tasks.back().death_thread_name = "WorkerPool/-1340960768";

    // Add a second snapshot.
    process_data.tasks.push_back(tracked_objects::TaskSnapshot());
    process_data.tasks.back().birth = child;
    process_data.tasks.back().death_data.count = 41;
    process_data.tasks.back().death_data.run_duration_max = 205;
    process_data.tasks.back().death_data.run_duration_sample = 203;
    process_data.tasks.back().death_data.run_duration_sum = 2017;
    process_data.tasks.back().death_data.queue_duration_max = 2053;
    process_data.tasks.back().death_data.queue_duration_sample = 2013;
    process_data.tasks.back().death_data.queue_duration_sum = 2079;
    process_data.tasks.back().death_thread_name = "PAC thread #3";

    // Add a parent-child pair.
    process_data.descendants.push_back(
        tracked_objects::ParentChildPairSnapshot());
    process_data.descendants.back().parent = parent;
    process_data.descendants.back().child = child;

    int process_type = content::PROCESS_TYPE_RENDERER;
    ExpectSerialization(process_data, process_type,
                       "{"
                          "\"descendants\":["
                             "{"
                               "\"child_location\":{"
                                  "\"file_name\":\"path/to/bar.cc\","
                                  "\"function_name\":\"FizzBoom\","
                                 "\"line_number\":433"
                               "},"
                               "\"child_thread\":\"Chrome_IOThread\","
                               "\"parent_location\":{"
                                  "\"file_name\":\"path/to/foo.cc\","
                                  "\"function_name\":\"WhizBang\","
                                  "\"line_number\":101"
                               "},"
                               "\"parent_thread\":\"CrBrowserMain\""
                             "}"
                          "],"
                          "\"list\":[{"
                             "\"birth_location\":{"
                                "\"file_name\":\"path/to/foo.cc\","
                                "\"function_name\":\"WhizBang\","
                                "\"line_number\":101"
                             "},"
                             "\"birth_thread\":\"CrBrowserMain\","
                             "\"death_data\":{"
                                "\"count\":37,"
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
                                "\"count\":41,"
                                "\"queue_ms\":2079,"
                                "\"queue_ms_max\":2053,"
                                "\"queue_ms_sample\":2013,"
                                "\"run_ms\":2017,"
                                "\"run_ms_max\":205,"
                                "\"run_ms_sample\":203"
                             "},"
                             "\"death_thread\":\"PAC thread #3\""
                          "}],"
                          "\"process_id\":" + GetProcessIdString() + ","
                          "\"process_type\":\"Tab\""
                        "}");
  }
}
