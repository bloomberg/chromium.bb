// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/devtools/v8_sampling_profiler.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;
using base::trace_event::CategoryFilter;
using base::trace_event::TraceLog;
using base::trace_event::TraceOptions;
using base::trace_event::TraceResultBuffer;

namespace content {

class V8SamplingProfilerTest : public RenderViewTest {
 public:
  void SetUp() override {
    RenderViewTest::SetUp();
    sampling_profiler_.reset(new V8SamplingProfiler(true));
    trace_buffer_.SetOutputCallback(json_output_.GetCallback());
  }

  void TearDown() override {
    sampling_profiler_.reset();
    RenderViewTest::TearDown();
  }

  void KickV8() { ExecuteJavaScript("1"); }

  void SyncFlush(TraceLog* trace_log) {
    base::WaitableEvent flush_complete_event(false, false);
    trace_log->Flush(
        base::Bind(&V8SamplingProfilerTest::OnTraceDataCollected,
                   base::Unretained(static_cast<V8SamplingProfilerTest*>(this)),
                   base::Unretained(&flush_complete_event)));
    base::RunLoop().RunUntilIdle();
    flush_complete_event.Wait();
  }

  void OnTraceDataCollected(
      base::WaitableEvent* flush_complete_event,
      const scoped_refptr<base::RefCountedString>& events_str,
      bool has_more_events) {
    base::AutoLock lock(lock_);
    json_output_.json_output.clear();
    trace_buffer_.Start();
    trace_buffer_.AddFragment(events_str->data());
    trace_buffer_.Finish();

    scoped_ptr<Value> root;
    root.reset(base::JSONReader::Read(
        json_output_.json_output,
        base::JSON_PARSE_RFC | base::JSON_DETACHABLE_CHILDREN));

    if (!root.get()) {
      LOG(ERROR) << json_output_.json_output;
    }

    ListValue* root_list = NULL;
    ASSERT_TRUE(root.get());
    ASSERT_TRUE(root->GetAsList(&root_list));

    // Move items into our aggregate collection
    while (root_list->GetSize()) {
      scoped_ptr<Value> item;
      root_list->Remove(0, &item);
      trace_parsed_.Append(item.release());
    }

    if (!has_more_events)
      flush_complete_event->Signal();
  }

  scoped_ptr<V8SamplingProfiler> sampling_profiler_;
  base::Lock lock_;

  ListValue trace_parsed_;
  TraceResultBuffer trace_buffer_;
  TraceResultBuffer::SimpleOutput json_output_;
};

TEST_F(V8SamplingProfilerTest, V8SamplingEventFired) {
  scoped_ptr<V8SamplingProfiler> sampling_profiler(
      new V8SamplingProfiler(true));
  sampling_profiler->EnableSamplingEventForTesting();
  TraceLog::GetInstance()->SetEnabled(
      CategoryFilter(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile")),
      TraceLog::RECORDING_MODE, TraceOptions());
  sampling_profiler->WaitSamplingEventForTesting();
  TraceLog::GetInstance()->SetDisabled();
}

TEST_F(V8SamplingProfilerTest, V8SamplingJitCodeEventsCollected) {
  TraceLog* trace_log = TraceLog::GetInstance();
  trace_log->SetEnabled(
      CategoryFilter(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile")),
      TraceLog::RECORDING_MODE, TraceOptions());
  KickV8();  // Make a call to V8 so it can invoke interrupt request callbacks.
  trace_log->SetDisabled();
  SyncFlush(trace_log);
  size_t trace_parsed_count = trace_parsed_.GetSize();
  int jit_code_added_events_count = 0;
  for (size_t i = 0; i < trace_parsed_count; i++) {
    const DictionaryValue* dict;
    if (!trace_parsed_.GetDictionary(i, &dict))
      continue;
    std::string value;
    if (!dict->GetString("cat", &value) ||
        value != TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"))
      continue;
    if (!dict->GetString("name", &value) || value != "JitCodeAdded")
      continue;
    ++jit_code_added_events_count;
  }
  CHECK_LT(0, jit_code_added_events_count);
  base::RunLoop().RunUntilIdle();
}

}  // namespace content
