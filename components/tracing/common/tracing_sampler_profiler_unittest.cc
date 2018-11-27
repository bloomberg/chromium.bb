// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/tracing_sampler_profiler.h"

#include "base/at_exit.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted_memory.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace {

using base::trace_event::TraceLog;
using base::StackSamplingProfiler;

class TracingSampleProfilerTest : public testing::Test {
 public:
  TracingSampleProfilerTest() : testing::Test() {}
  ~TracingSampleProfilerTest() override {}

  void SetUp() override {
    TraceLog* tracelog = TraceLog::GetInstance();
    ASSERT_TRUE(tracelog);
    ASSERT_FALSE(tracelog->IsEnabled());
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
  }

  void TearDown() override {
    EXPECT_FALSE(TraceLog::GetInstance()->IsEnabled());

    // Be sure there is no pending/running tasks.
    scoped_task_environment_.RunUntilIdle();
  }

  void BeginTrace() {
    base::trace_event::TraceConfig config(
        TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
        base::trace_event::RECORD_UNTIL_FULL);
    TraceLog::GetInstance()->SetEnabled(config, TraceLog::RECORDING_MODE);
    EXPECT_TRUE(TraceLog::GetInstance()->IsEnabled());
  }

  void WaitForEvents() {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(200));
  }

  // Returns whether of not the sampler profiling is able to unwind the stack
  // on this platform.
  bool IsStackUnwindingSupported() {
#if defined(OS_MACOSX) || defined(OS_WIN) && defined(_WIN64) ||     \
    (defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
     defined(OFFICIAL_BUILD))
    return true;
#else
    return false;
#endif
  }

  static void TraceDataCallback(
      const base::RepeatingCallback<void()>& callback,
      std::string* output,
      const scoped_refptr<base::RefCountedString>& json_events_str,
      bool has_more_events) {
    if (output->size() > 1 && !json_events_str->data().empty()) {
      output->append(",");
    }
    output->append(json_events_str->data());
    if (!has_more_events) {
      callback.Run();
    }
  }

  void EndTracing() {
    std::string json_data = "[";
    TraceLog::GetInstance()->SetDisabled();
    base::RunLoop run_loop;
    TraceLog::GetInstance()->Flush(base::BindRepeating(
        &TracingSampleProfilerTest::TraceDataCallback, run_loop.QuitClosure(),
        base::Unretained(&json_data)));
    run_loop.Run();
    json_data.append("]");

    std::string error_msg;
    std::unique_ptr<base::Value> trace_data =
        base::JSONReader::ReadAndReturnError(json_data, 0, nullptr, &error_msg);
    CHECK(trace_data) << "JSON parsing failed (" << error_msg << ")";

    base::ListValue* list;
    CHECK(trace_data->GetAsList(&list));
    for (size_t i = 0; i < list->GetSize(); i++) {
      base::Value* item = nullptr;
      if (list->Get(i, &item)) {
        base::DictionaryValue* dict;
        CHECK(item->GetAsDictionary(&dict));
        std::string name;
        CHECK(dict->GetString("name", &name));
        if (name == "StackCpuSampling") {
          events_stack_received_count_++;
        } else if (name == "ProcessPriority") {
          events_priority_received_count_++;
        }
      }
    }
  }

  void ValidateReceivedEvents() {
    if (IsStackUnwindingSupported()) {
      EXPECT_GT(events_stack_received_count_, 0U);
      EXPECT_GT(events_priority_received_count_, 0U);
    } else {
      EXPECT_EQ(events_stack_received_count_, 0U);
      EXPECT_EQ(events_priority_received_count_, 0U);
    }
  }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // We want our singleton torn down after each test.
  base::ShadowingAtExitManager at_exit_manager_;
  base::trace_event::TraceResultBuffer trace_buffer_;
  base::trace_event::TraceResultBuffer::SimpleOutput json_output_;

  // Number of stack sampling events received.
  size_t events_stack_received_count_ = 0;

  // Number of priority sampling events received.
  size_t events_priority_received_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TracingSampleProfilerTest);
};

}  // namespace

TEST_F(TracingSampleProfilerTest, OnSampleCompleted) {
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  profiler->OnMessageLoopStarted();
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

TEST_F(TracingSampleProfilerTest, JoinRunningTracing) {
  BeginTrace();
  auto profiler = TracingSamplerProfiler::CreateOnMainThread();
  profiler->OnMessageLoopStarted();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  base::RunLoop().RunUntilIdle();
  ValidateReceivedEvents();
}

TEST_F(TracingSampleProfilerTest, SamplingChildThread) {
  base::Thread sampled_thread("sampling_profiler_test");
  sampled_thread.Start();
  sampled_thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TracingSamplerProfiler::CreateOnChildThread));
  BeginTrace();
  base::RunLoop().RunUntilIdle();
  WaitForEvents();
  EndTracing();
  ValidateReceivedEvents();
}

}  // namespace tracing
