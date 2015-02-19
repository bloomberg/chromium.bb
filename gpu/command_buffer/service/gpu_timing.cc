// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_timing.h"

#include "base/time/time.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

GPUTimer::GPUTimer(GPUTiming* gpu_timing) : gpu_timing_(gpu_timing) {
  DCHECK(gpu_timing_);
  memset(queries_, 0, sizeof(queries_));
  glGenQueriesARB(2, queries_);
}

GPUTimer::~GPUTimer() {
  glDeleteQueriesARB(2, queries_);
}

void GPUTimer::Start() {
  // GL_TIMESTAMP and GL_TIMESTAMP_EXT both have the same value.
  glQueryCounter(queries_[0], GL_TIMESTAMP);
}

void GPUTimer::End() {
  end_requested_ = true;
  offset_ = gpu_timing_->CalculateTimerOffset();
  glQueryCounter(queries_[1], GL_TIMESTAMP);
}

bool GPUTimer::IsAvailable() {
  if (!gpu_timing_->IsAvailable() || !end_requested_) {
    return false;
  }
  GLint done = 0;
  glGetQueryObjectivARB(queries_[1], GL_QUERY_RESULT_AVAILABLE, &done);
  return done != 0;
}

void GPUTimer::GetStartEndTimestamps(int64* start, int64* end) {
  DCHECK(start && end);
  DCHECK(IsAvailable());
  GLuint64 begin_stamp = 0;
  GLuint64 end_stamp = 0;
  // TODO(dsinclair): It's possible for the timer to wrap during the start/end.
  // We need to detect if the end is less then the start and correct for the
  // wrapping.
  glGetQueryObjectui64v(queries_[0], GL_QUERY_RESULT, &begin_stamp);
  glGetQueryObjectui64v(queries_[1], GL_QUERY_RESULT, &end_stamp);

  *start = (begin_stamp / base::Time::kNanosecondsPerMicrosecond) + offset_;
  *end = (end_stamp / base::Time::kNanosecondsPerMicrosecond) + offset_;
}

int64 GPUTimer::GetDeltaElapsed() {
  int64 start = 0;
  int64 end = 0;
  GetStartEndTimestamps(&start, &end);
  return end - start;
}

GPUTiming::GPUTiming() : cpu_time_for_testing_() {
}

GPUTiming::~GPUTiming() {
}

bool GPUTiming::Initialize(gfx::GLContext* gl_context) {
  DCHECK(gl_context);
  DCHECK_EQ(kTimerTypeInvalid, timer_type_);

  const gfx::GLVersionInfo* version_info = gl_context->GetVersionInfo();
  DCHECK(version_info);
  if (version_info->is_es3 &&  // glGetInteger64v is supported under ES3.
      gfx::g_driver_gl.ext.b_GL_EXT_disjoint_timer_query) {
    timer_type_ = kTimerTypeDisjoint;
    return true;
  } else if (gfx::g_driver_gl.ext.b_GL_ARB_timer_query) {
    timer_type_ = kTimerTypeARB;
    return true;
  }
  return false;
}

bool GPUTiming::IsAvailable() {
  return timer_type_ != kTimerTypeInvalid;
}

const char* GPUTiming::GetTimerTypeName() const {
  switch (timer_type_) {
    case kTimerTypeDisjoint:
      return "GL_EXT_disjoint_timer_query";
    case kTimerTypeARB:
      return "GL_ARB_timer_query";
    default:
      return "Unknown";
  }
}

bool GPUTiming::CheckAndResetTimerErrors() {
  if (timer_type_ == kTimerTypeDisjoint) {
    GLint disjoint_value = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
    return disjoint_value != 0;
  } else {
    return false;
  }
}

int64 GPUTiming::CalculateTimerOffset() {
  if (!offset_valid_) {
    GLint64 gl_now = 0;
    glGetInteger64v(GL_TIMESTAMP, &gl_now);
    int64 now =
        cpu_time_for_testing_.is_null()
            ? base::TimeTicks::NowFromSystemTraceTime().ToInternalValue()
            : cpu_time_for_testing_.Run();
    offset_ = now - gl_now / base::Time::kNanosecondsPerMicrosecond;
    offset_valid_ = timer_type_ == kTimerTypeARB;
  }
  return offset_;
}

void GPUTiming::InvalidateTimerOffset() {
  offset_valid_ = false;
}

void GPUTiming::SetCpuTimeForTesting(
    const base::Callback<int64(void)>& cpu_time) {
  cpu_time_for_testing_ = cpu_time;
}

void GPUTiming::SetOffsetForTesting(int64 offset, bool cache_it) {
  offset_ = offset;
  offset_valid_ = cache_it;
}

void GPUTiming::SetTimerTypeForTesting(TimerType type) {
  timer_type_ = type;
}

}  // namespace gpu
