// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_tracer.h"

#include <deque>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/memory/weak_ptr.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {
namespace gles2 {
namespace {

class Outputter;

static const unsigned int kProcessInterval = 16;
static Outputter* g_outputter_thread = NULL;

class Outputter
    : private base::Thread,
      public base::RefCounted<Outputter> {
 public:
  static scoped_refptr<Outputter> Create(const std::string& name) {
    if (!g_outputter_thread) {
      g_outputter_thread = new Outputter(name);
      g_outputter_thread->Start();
      g_outputter_thread->Stop();
    }
    return g_outputter_thread;
  }

  uint64 Id() { return thread_id(); }

 private:
  friend class base::RefCounted<Outputter>;

  explicit Outputter(const std::string& name) : base::Thread(name.c_str()) {}

  virtual ~Outputter() {
    g_outputter_thread = NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(Outputter);
};

class Trace : public base::RefCounted<Trace> {
 public:
  explicit Trace(const std::string& name) : name_(name) {}

  virtual void Start() = 0;
  virtual void End() = 0;

  // True if the the results of this query are available.
  virtual bool IsAvailable() = 0;

  virtual bool IsProcessable() { return true; }
  virtual void Process() = 0;

  virtual const std::string& name() {
    return name_;
  }

 protected:
  virtual ~Trace() {}

 private:
  friend class base::RefCounted<Trace>;

  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(Trace);
};

class GLARBTimerTrace : public Trace {
 public:
  GLARBTimerTrace(scoped_refptr<Outputter> outputter, const std::string& name,
                  int64 offset);

  // Implementation of Tracer
  virtual void Start() OVERRIDE;
  virtual void End() OVERRIDE;
  virtual bool IsAvailable() OVERRIDE;
  virtual void Process() OVERRIDE;

 private:
  ~GLARBTimerTrace();

  void Output();

  scoped_refptr<Outputter> outputter_;

  int64 offset_;
  int64 start_time_;
  int64 end_time_;
  bool end_requested_;

  GLuint queries_[2];

  DISALLOW_COPY_AND_ASSIGN(GLARBTimerTrace);
};

class NoopTrace : public Trace {
 public:
  explicit NoopTrace(const std::string& name) : Trace(name) {}

  // Implementation of Tracer
  virtual void Start() OVERRIDE {}
  virtual void End() OVERRIDE {}
  virtual bool IsAvailable() OVERRIDE { return true; }
  virtual bool IsProcessable() OVERRIDE { return false; }
  virtual void Process() OVERRIDE {}

 private:
  ~NoopTrace() {}

  DISALLOW_COPY_AND_ASSIGN(NoopTrace);
};

class GPUTracerImpl
    : public GPUTracer,
      public base::SupportsWeakPtr<GPUTracerImpl> {
 public:
  GPUTracerImpl()
      : gpu_category_enabled_(TRACE_EVENT_API_GET_CATEGORY_ENABLED("gpu")),
        process_posted_(false) {
  }
  virtual ~GPUTracerImpl() {}

  // Implementation of gpu::gles2::GPUTracer
  virtual bool Begin(const std::string& name) OVERRIDE;
  virtual bool End() OVERRIDE;
  virtual const std::string& CurrentName() const OVERRIDE;

  // Process any completed traces.
  virtual void Process();

 protected:
  // Create a new trace.
  virtual scoped_refptr<Trace> CreateTrace(const std::string& name);

  const unsigned char* gpu_category_enabled_;

 private:
  void IssueProcessTask();

  scoped_refptr<Trace> current_trace_;
  std::deque<scoped_refptr<Trace> > traces_;

  bool process_posted_;

  DISALLOW_COPY_AND_ASSIGN(GPUTracerImpl);
};

class GPUTracerARBTimerQuery : public GPUTracerImpl {
 public:
  GPUTracerARBTimerQuery();
  virtual ~GPUTracerARBTimerQuery();

  // Implementation of GPUTracerImpl
  virtual void Process() OVERRIDE;

 private:
  // Implementation of GPUTracerImpl.
  virtual scoped_refptr<Trace> CreateTrace(const std::string& name) OVERRIDE;

  void CalculateTimerOffset();

  scoped_refptr<Outputter> outputter_;

  int64 timer_offset_;
  int64 last_offset_check_;

  DISALLOW_COPY_AND_ASSIGN(GPUTracerARBTimerQuery);
};

GLARBTimerTrace::GLARBTimerTrace(scoped_refptr<Outputter> outputter,
                                 const std::string& name, int64 offset)
    : Trace(name),
      outputter_(outputter),
      offset_(offset),
      start_time_(0),
      end_time_(0),
      end_requested_(false) {
  glGenQueries(2, queries_);
}

GLARBTimerTrace::~GLARBTimerTrace() {
}

void GLARBTimerTrace::Start() {
  glQueryCounter(queries_[0], GL_TIMESTAMP);
}

void GLARBTimerTrace::End() {
  glQueryCounter(queries_[1], GL_TIMESTAMP);
  end_requested_ = true;
}

bool GLARBTimerTrace::IsAvailable() {
  if (!end_requested_)
    return false;

  GLint done = 0;
  glGetQueryObjectiv(queries_[1], GL_QUERY_RESULT_AVAILABLE, &done);
  return !!done;
}

void GLARBTimerTrace::Process() {
  DCHECK(IsAvailable());

  GLint64 timestamp;

  // TODO(dsinclair): It's possible for the timer to wrap during the start/end.
  // We need to detect if the end is less then the start and correct for the
  // wrapping.
  glGetQueryObjecti64v(queries_[0], GL_QUERY_RESULT, &timestamp);
  start_time_ = (timestamp / base::Time::kNanosecondsPerMicrosecond) + offset_;

  glGetQueryObjecti64v(queries_[1], GL_QUERY_RESULT, &timestamp);
  end_time_ = (timestamp / base::Time::kNanosecondsPerMicrosecond) + offset_;

  glDeleteQueries(2, queries_);

  TRACE_EVENT_COPY_BEGIN_WITH_ID_TID_AND_TIMESTAMP0("gpu", name().c_str(),
                                   this, outputter_->Id(), start_time_);
  TRACE_EVENT_COPY_END_WITH_ID_TID_AND_TIMESTAMP0("gpu", name().c_str(),
                                   this, outputter_->Id(), end_time_);
}

bool GPUTracerImpl::Begin(const std::string& name) {
  // Make sure we are not nesting trace commands.
  if (current_trace_)
    return false;

  current_trace_ = CreateTrace(name);
  current_trace_->Start();
  return true;
}

bool GPUTracerImpl::End() {
  if (!current_trace_)
    return false;

  current_trace_->End();
  if (current_trace_->IsProcessable())
    traces_.push_back(current_trace_);
  current_trace_ = NULL;

  IssueProcessTask();
  return true;
}

void GPUTracerImpl::Process() {
  process_posted_ = false;

  while (!traces_.empty() && traces_.front()->IsAvailable()) {
    traces_.front()->Process();
    traces_.pop_front();
  }

  IssueProcessTask();
}

const std::string& GPUTracerImpl::CurrentName() const {
  if (!current_trace_)
    return EmptyString();
  return current_trace_->name();
}

scoped_refptr<Trace> GPUTracerImpl::CreateTrace(const std::string& name) {
  return new NoopTrace(name);
}

void GPUTracerImpl::IssueProcessTask() {
  if (traces_.empty() || process_posted_)
    return;

  process_posted_ = true;
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&GPUTracerImpl::Process, base::AsWeakPtr(this)),
      base::TimeDelta::FromMilliseconds(kProcessInterval));
}

GPUTracerARBTimerQuery::GPUTracerARBTimerQuery()
    : GPUTracerImpl(),
      timer_offset_(0),
      last_offset_check_(0) {
  CalculateTimerOffset();
  outputter_ = Outputter::Create("GL_ARB_timer_query");
}

GPUTracerARBTimerQuery::~GPUTracerARBTimerQuery() {
}

scoped_refptr<Trace> GPUTracerARBTimerQuery::CreateTrace(
    const std::string& name) {
  if (*gpu_category_enabled_)
    return new GLARBTimerTrace(outputter_, name, timer_offset_);
  return GPUTracerImpl::CreateTrace(name);
}

void GPUTracerARBTimerQuery::Process() {
  GPUTracerImpl::Process();

  if (*gpu_category_enabled_ &&
      (last_offset_check_ + base::Time::kMicrosecondsPerSecond) <
          base::TimeTicks::NowFromSystemTraceTime().ToInternalValue())
    CalculateTimerOffset();
}

void GPUTracerARBTimerQuery::CalculateTimerOffset() {
  TRACE_EVENT0("gpu", "CalculateTimerOffset");
  // TODO(dsinclair): Change to glGetInteger64v.
  GLuint64 gl_now = 0;
  GLuint query;
  glGenQueries(1, &query);

  glQueryCounter(query, GL_TIMESTAMP);
  glGetQueryObjectui64v(query, GL_QUERY_RESULT, &gl_now);
  base::TimeTicks system_now = base::TimeTicks::NowFromSystemTraceTime();

  gl_now /= base::Time::kNanosecondsPerMicrosecond;
  timer_offset_ = system_now.ToInternalValue() - gl_now;
  glDeleteQueries(1, &query);

  last_offset_check_ = system_now.ToInternalValue();
}

}  // namespace

scoped_ptr<GPUTracer> GPUTracer::Create() {
  if (gfx::g_driver_gl.ext.b_GL_ARB_timer_query)
    return scoped_ptr<GPUTracer>(new GPUTracerARBTimerQuery());
  return scoped_ptr<GPUTracer>(new GPUTracerImpl());
}

}  // namespace gles2
}  // namespace gpu
