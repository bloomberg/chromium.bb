// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_tracer.h"

#include <deque>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"

namespace gpu {
namespace gles2 {

static const unsigned int kProcessInterval = 16;
static TraceOutputter* g_outputter_thread = NULL;

TraceMarker::TraceMarker(const std::string& name)
    : name_(name),
      trace_(NULL) {
}

TraceMarker::~TraceMarker() {
}

scoped_refptr<TraceOutputter> TraceOutputter::Create(const std::string& name) {
  if (!g_outputter_thread) {
    g_outputter_thread = new TraceOutputter(name);
  }
  return g_outputter_thread;
}

TraceOutputter::TraceOutputter(const std::string& name)
    : named_thread_(name.c_str()), local_trace_id_(0) {
  named_thread_.Start();
  named_thread_.Stop();
}

TraceOutputter::~TraceOutputter() { g_outputter_thread = NULL; }

void TraceOutputter::Trace(const std::string& name,
                           int64 start_time,
                           int64 end_time) {
  TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"),
      name.c_str(),
      local_trace_id_,
      named_thread_.thread_id(),
      start_time);
  TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0(
      TRACE_DISABLED_BY_DEFAULT("gpu.device"),
      name.c_str(),
      local_trace_id_,
      named_thread_.thread_id(),
      end_time);
  ++local_trace_id_;
}

GPUTrace::GPUTrace(scoped_refptr<Outputter> outputter,
                   const std::string& name,
                   int64 offset,
                   GpuTracerType tracer_type)
    : name_(name),
      outputter_(outputter),
      offset_(offset),
      start_time_(0),
      end_time_(0),
      tracer_type_(tracer_type),
      end_requested_(false) {
  memset(queries_, 0, sizeof(queries_));
  switch (tracer_type_) {
    case kTracerTypeARBTimer:
    case kTracerTypeDisjointTimer:
      glGenQueriesARB(2, queries_);
      break;

    default:
      tracer_type_ = kTracerTypeInvalid;
  }
}

GPUTrace::~GPUTrace() {
  switch (tracer_type_) {
    case kTracerTypeInvalid:
      break;

    case kTracerTypeARBTimer:
    case kTracerTypeDisjointTimer:
      glDeleteQueriesARB(2, queries_);
      break;
  }
}

void GPUTrace::Start() {
  TRACE_EVENT_COPY_ASYNC_BEGIN0(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"), name().c_str(), this);

  switch (tracer_type_) {
    case kTracerTypeInvalid:
      break;

    case kTracerTypeDisjointTimer:
      // For the disjoint timer, GPU idle time does not seem to increment the
      // internal counter. We must calculate the offset before any query. The
      // good news is any device that supports disjoint timer will also support
      // glGetInteger64v, so we can query it directly unlike the ARBTimer case.
      // The "offset_" variable will always be 0 during normal use cases, only
      // under the unit tests will it be set to specific test values.
      if (offset_ == 0) {
        GLint64 gl_now = 0;
        glGetInteger64v(GL_TIMESTAMP, &gl_now);
        offset_ = base::TimeTicks::NowFromSystemTraceTime().ToInternalValue() -
                  gl_now / base::Time::kNanosecondsPerMicrosecond;
      }
      // Intentionally fall through to kTracerTypeARBTimer case.xs
    case kTracerTypeARBTimer:
      // GL_TIMESTAMP and GL_TIMESTAMP_EXT both have the same value.
      glQueryCounter(queries_[0], GL_TIMESTAMP);
      break;
  }
}

void GPUTrace::End() {
  end_requested_ = true;
  switch (tracer_type_) {
    case kTracerTypeInvalid:
      break;

    case kTracerTypeARBTimer:
    case kTracerTypeDisjointTimer:
      // GL_TIMESTAMP and GL_TIMESTAMP_EXT both have the same value.
      glQueryCounter(queries_[1], GL_TIMESTAMP);
      break;
  }

  TRACE_EVENT_COPY_ASYNC_END0(
      TRACE_DISABLED_BY_DEFAULT("gpu.service"), name().c_str(), this);
}

bool GPUTrace::IsAvailable() {
  if (tracer_type_ != kTracerTypeInvalid) {
    if (!end_requested_)
      return false;

    GLint done = 0;
    glGetQueryObjectiv(queries_[1], GL_QUERY_RESULT_AVAILABLE, &done);
    return !!done;
  }

  return true;
}

void GPUTrace::Process() {
  if (tracer_type_ == kTracerTypeInvalid)
    return;

  DCHECK(IsAvailable());

  GLuint64 begin_stamp = 0;
  GLuint64 end_stamp = 0;

  // TODO(dsinclair): It's possible for the timer to wrap during the start/end.
  // We need to detect if the end is less then the start and correct for the
  // wrapping.
  glGetQueryObjectui64v(queries_[0], GL_QUERY_RESULT, &begin_stamp);
  glGetQueryObjectui64v(queries_[1], GL_QUERY_RESULT, &end_stamp);

  start_time_ = (begin_stamp / base::Time::kNanosecondsPerMicrosecond) +
                offset_;
  end_time_ = (end_stamp / base::Time::kNanosecondsPerMicrosecond) + offset_;
  outputter_->Trace(name(), start_time_, end_time_);
}

GPUTracer::GPUTracer(gles2::GLES2Decoder* decoder)
    : gpu_trace_srv_category(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.service"))),
      gpu_trace_dev_category(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("gpu.device"))),
      decoder_(decoder),
      timer_offset_(0),
      last_tracer_source_(kTraceGroupInvalid),
      tracer_type_(kTracerTypeInvalid),
      gpu_timing_synced_(false),
      gpu_executing_(false),
      process_posted_(false) {
  if (gfx::g_driver_gl.ext.b_GL_EXT_disjoint_timer_query) {
    tracer_type_ = kTracerTypeDisjointTimer;
    outputter_ = TraceOutputter::Create("GL_EXT_disjoint_timer_query");
  } else if (gfx::g_driver_gl.ext.b_GL_ARB_timer_query) {
    tracer_type_ = kTracerTypeARBTimer;
    outputter_ = TraceOutputter::Create("GL_ARB_timer_query");
  }
}

GPUTracer::~GPUTracer() {
}

bool GPUTracer::BeginDecoding() {
  if (gpu_executing_)
    return false;

  CalculateTimerOffset();
  gpu_executing_ = true;

  if (IsTracing()) {
    // Reset disjoint bit for the disjoint timer.
    if (tracer_type_ == kTracerTypeDisjointTimer) {
      GLint disjoint_value = 0;
      glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
    }

    // Begin a Trace for all active markers
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      for (size_t i = 0; i < markers_[n].size(); i++) {
        markers_[n][i].trace_ = CreateTrace(markers_[n][i].name_);
        markers_[n][i].trace_->Start();
      }
    }
  }
  return true;
}

bool GPUTracer::EndDecoding() {
  if (!gpu_executing_)
    return false;

  // End Trace for all active markers
  if (IsTracing()) {
    for (int n = 0; n < NUM_TRACER_SOURCES; n++) {
      for (size_t i = 0; i < markers_[n].size(); i++) {
        if (markers_[n][i].trace_.get()) {
          markers_[n][i].trace_->End();
          if (markers_[n][i].trace_->IsEnabled())
            traces_.push_back(markers_[n][i].trace_);
          markers_[n][i].trace_ = 0;
        }
      }
    }
    IssueProcessTask();
  }

  gpu_executing_ = false;

  // NOTE(vmiura): glFlush() here can help give better trace results,
  // but it distorts the normal device behavior.
  return true;
}

bool GPUTracer::Begin(const std::string& name, GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Push new marker from given 'source'
  last_tracer_source_ = source;
  markers_[source].push_back(TraceMarker(name));

  // Create trace
  if (IsTracing()) {
    scoped_refptr<GPUTrace> trace = CreateTrace(name);
    trace->Start();
    markers_[source].back().trace_ = trace;
  }

  return true;
}

bool GPUTracer::End(GpuTracerSource source) {
  if (!gpu_executing_)
    return false;

  DCHECK(source >= 0 && source < NUM_TRACER_SOURCES);

  // Pop last marker with matching 'source'
  if (!markers_[source].empty()) {
    if (IsTracing()) {
      scoped_refptr<GPUTrace> trace = markers_[source].back().trace_;
      if (trace.get()) {
        trace->End();
        if (trace->IsEnabled())
          traces_.push_back(trace);
        IssueProcessTask();
      }
    }

    markers_[source].pop_back();
    return true;
  }
  return false;
}

bool GPUTracer::IsTracing() {
  return (*gpu_trace_srv_category != 0) || (*gpu_trace_dev_category != 0);
}

const std::string& GPUTracer::CurrentName() const {
  if (last_tracer_source_ >= 0 &&
      last_tracer_source_ < NUM_TRACER_SOURCES &&
      !markers_[last_tracer_source_].empty()) {
    return markers_[last_tracer_source_].back().name_;
  }
  return base::EmptyString();
}

scoped_refptr<GPUTrace> GPUTracer::CreateTrace(const std::string& name) {
  GpuTracerType tracer_type = *gpu_trace_dev_category ? tracer_type_ :
                                                        kTracerTypeInvalid;

  return new GPUTrace(outputter_, name, timer_offset_, tracer_type);
}

void GPUTracer::Process() {
  process_posted_ = false;
  ProcessTraces();
  IssueProcessTask();
}

void GPUTracer::ProcessTraces() {
  if (tracer_type_ == kTracerTypeInvalid) {
    traces_.clear();
    return;
  }

  TRACE_EVENT0("gpu", "GPUTracer::ProcessTraces");

  // Make owning decoder's GL context current
  if (!decoder_->MakeCurrent()) {
    // Skip subsequent GL calls if MakeCurrent fails
    traces_.clear();
    return;
  }

  // Check if disjoint operation has occurred, discard ongoing traces if so.
  if (tracer_type_ == kTracerTypeDisjointTimer) {
    GLint disjoint_value = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint_value);
    if (disjoint_value)
      traces_.clear();
  }

  while (!traces_.empty() && traces_.front()->IsAvailable()) {
    traces_.front()->Process();
    traces_.pop_front();
  }

  // Clear pending traces if there were are any errors
  GLenum err = glGetError();
  if (err != GL_NO_ERROR)
    traces_.clear();
}

void GPUTracer::CalculateTimerOffset() {
  if (tracer_type_ != kTracerTypeInvalid) {
    if (*gpu_trace_dev_category == '\0') {
      // If GPU device category is off, invalidate timing sync.
      gpu_timing_synced_ = false;
      return;
    } else if (tracer_type_ == kTracerTypeDisjointTimer) {
      // Disjoint timers offsets should be calculated before every query.
      gpu_timing_synced_ = true;
      timer_offset_ = 0;
    }

    if (gpu_timing_synced_)
      return;

    TRACE_EVENT0("gpu", "GPUTracer::CalculateTimerOffset");

    // NOTE(vmiura): It would be better to use glGetInteger64v, however
    // it's not available everywhere.
    GLuint64 gl_now = 0;
    GLuint query;

    glGenQueriesARB(1, &query);

    glFinish();
    glQueryCounter(query, GL_TIMESTAMP);
    glFinish();

    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &gl_now);
    glDeleteQueriesARB(1, &query);

    base::TimeTicks system_now = base::TimeTicks::NowFromSystemTraceTime();

    gl_now /= base::Time::kNanosecondsPerMicrosecond;
    timer_offset_ = system_now.ToInternalValue() - gl_now;
    gpu_timing_synced_ = true;
  }
}

void GPUTracer::IssueProcessTask() {
  if (traces_.empty() || process_posted_)
    return;

  process_posted_ = true;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GPUTracer::Process, base::AsWeakPtr(this)),
      base::TimeDelta::FromMilliseconds(kProcessInterval));
}

}  // namespace gles2
}  // namespace gpu
