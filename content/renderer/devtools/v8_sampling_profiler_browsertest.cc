// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event.h"
#include "content/renderer/devtools/v8_sampling_profiler.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::trace_event::CategoryFilter;
using base::trace_event::TraceLog;
using base::trace_event::TraceOptions;

namespace content {

class V8SamplingProfilerTest : public testing::Test {};

TEST_F(V8SamplingProfilerTest, V8SamplingEventFired) {
  scoped_ptr<V8SamplingProfiler> sampling_profiler(new V8SamplingProfiler());
  sampling_profiler->EnableSamplingEventForTesting();
  TraceLog::GetInstance()->SetEnabled(
      CategoryFilter(TRACE_DISABLED_BY_DEFAULT("v8_cpu_profile")),
      TraceLog::RECORDING_MODE, TraceOptions());
  sampling_profiler->WaitSamplingEventForTesting();
  TraceLog::GetInstance()->SetDisabled();
}

}  // namespace content
