// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/v8_sampling_profiler.h"

#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "content/renderer/render_thread_impl.h"
#include "v8/include/v8.h"

using base::trace_event::TraceLog;
using base::trace_event::TracedValue;
using v8::Isolate;

namespace content {

namespace {

std::string PtrToString(const void* value) {
  char buffer[20];
  base::snprintf(buffer, sizeof(buffer), "0x%" PRIx64,
                 static_cast<uint64>(reinterpret_cast<intptr_t>(value)));
  return buffer;
}

}  // namespace

// The class implements a sampler responsible for sampling a single thread.
class Sampler {
 public:
  explicit Sampler(Isolate* isolate) : isolate_(isolate) { DCHECK(isolate_); }

  static scoped_ptr<Sampler> CreateForCurrentThread();

  // These methods are called from the sampling thread.
  void Start();
  void Stop();
  void Sample();

 private:
  static void InstallJitCodeEventHandler(Isolate* isolate, void* data);
  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);
  static scoped_refptr<base::trace_event::ConvertableToTraceFormat>
      JitCodeEventToTraceFormat(const v8::JitCodeEvent* event);

  Isolate* isolate_;
};

// static
scoped_ptr<Sampler> Sampler::CreateForCurrentThread() {
  return scoped_ptr<Sampler>(new Sampler(Isolate::GetCurrent()));
}

void Sampler::Start() {
  v8::JitCodeEventHandler handler = &HandleJitCodeEvent;
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler,
                             reinterpret_cast<void*>(handler));
}

void Sampler::Stop() {
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler, nullptr);
}

void Sampler::Sample() {
}

// static
void Sampler::InstallJitCodeEventHandler(Isolate* isolate, void* data) {
  // Called on the sampled V8 thread.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
               "Sampler::InstallJitCodeEventHandler");
  v8::JitCodeEventHandler handler =
      reinterpret_cast<v8::JitCodeEventHandler>(data);
  isolate->SetJitCodeEventHandler(
      v8::JitCodeEventOptions::kJitCodeEventEnumExisting, handler);
}

// static
void Sampler::HandleJitCodeEvent(const v8::JitCodeEvent* event) {
  // Called on the sampled V8 thread.
  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED:
      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                           "JitCodeAdded", TRACE_EVENT_SCOPE_THREAD, "data",
                           JitCodeEventToTraceFormat(event));
      break;

    case v8::JitCodeEvent::CODE_MOVED:
      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                           "JitCodeMoved", TRACE_EVENT_SCOPE_THREAD, "data",
                           JitCodeEventToTraceFormat(event));
      break;

    case v8::JitCodeEvent::CODE_REMOVED:
      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                           "JitCodeRemoved", TRACE_EVENT_SCOPE_THREAD, "data",
                           JitCodeEventToTraceFormat(event));
      break;

    case v8::JitCodeEvent::CODE_ADD_LINE_POS_INFO:
    case v8::JitCodeEvent::CODE_START_LINE_INFO_RECORDING:
    case v8::JitCodeEvent::CODE_END_LINE_INFO_RECORDING:
      break;
  }
}

// static
scoped_refptr<base::trace_event::ConvertableToTraceFormat>
Sampler::JitCodeEventToTraceFormat(const v8::JitCodeEvent* event) {
  // Called on the sampled thread.
  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED: {
      scoped_refptr<TracedValue> data(new TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      data->SetString("name", std::string(event->name.str, event->name.len));
      return data;
    }

    case v8::JitCodeEvent::CODE_MOVED: {
      scoped_refptr<TracedValue> data(new TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      data->SetString("new_code_start", PtrToString(event->new_code_start));
      return data;
    }

    case v8::JitCodeEvent::CODE_REMOVED: {
      scoped_refptr<TracedValue> data(new TracedValue());
      data->SetString("code_start", PtrToString(event->code_start));
      data->SetInteger("code_len", static_cast<unsigned>(event->code_len));
      return data;
    }

    case v8::JitCodeEvent::CODE_ADD_LINE_POS_INFO:
    case v8::JitCodeEvent::CODE_START_LINE_INFO_RECORDING:
    case v8::JitCodeEvent::CODE_END_LINE_INFO_RECORDING:
      return nullptr;
  }
  return nullptr;
}

class V8SamplingThread : public base::PlatformThread::Delegate {
 public:
  V8SamplingThread(Sampler*, base::WaitableEvent*);

  // Implementation of PlatformThread::Delegate:
  void ThreadMain() override;

  void Start();
  void Stop();

 private:
  void Sample();
  void InstallSamplers();
  void RemoveSamplers();
  void StartSamplers();
  void StopSamplers();
  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);

  Sampler* render_thread_sampler_;
  base::CancellationFlag cancellation_flag_;
  base::WaitableEvent* waitable_event_for_testing_;
  base::PlatformThreadHandle sampling_thread_handle_;
  std::vector<Sampler*> samplers_;

  DISALLOW_COPY_AND_ASSIGN(V8SamplingThread);
};

V8SamplingThread::V8SamplingThread(Sampler* render_thread_sampler,
                                   base::WaitableEvent* event)
    : render_thread_sampler_(render_thread_sampler),
      waitable_event_for_testing_(event) {
}

void V8SamplingThread::ThreadMain() {
  base::PlatformThread::SetName("V8SamplingProfilerThread");
  InstallSamplers();
  StartSamplers();
  const int kSamplingFrequencyMicroseconds = 1000;
  while (!cancellation_flag_.IsSet()) {
    Sample();
    if (waitable_event_for_testing_) {
      waitable_event_for_testing_->Signal();
    }
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
  }
  StopSamplers();
  RemoveSamplers();
}

void V8SamplingThread::Sample() {
  for (Sampler* sampler : samplers_) {
    sampler->Sample();
  }
}

void V8SamplingThread::InstallSamplers() {
  // Note that the list does not own samplers.
  samplers_.push_back(render_thread_sampler_);
  // TODO: add worker samplers.
}

void V8SamplingThread::RemoveSamplers() {
  samplers_.clear();
}

void V8SamplingThread::StartSamplers() {
  for (Sampler* sampler : samplers_) {
    sampler->Start();
  }
}

void V8SamplingThread::StopSamplers() {
  for (Sampler* sampler : samplers_) {
    sampler->Stop();
  }
}

void V8SamplingThread::Start() {
  if (!base::PlatformThread::Create(0, this, &sampling_thread_handle_)) {
    DCHECK(false) << "failed to create thread";
  }
}

void V8SamplingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(sampling_thread_handle_);
}

V8SamplingProfiler::V8SamplingProfiler(bool underTest)
    : sampling_thread_(nullptr),
      render_thread_sampler_(Sampler::CreateForCurrentThread()) {
  DCHECK(underTest || RenderThreadImpl::current());
  // Force the "v8.cpu_profile" category to show up in the trace viewer.
  TraceLog::GetCategoryGroupEnabled(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"));
  TraceLog::GetInstance()->AddEnabledStateObserver(this);
}

V8SamplingProfiler::~V8SamplingProfiler() {
  TraceLog::GetInstance()->RemoveEnabledStateObserver(this);
  DCHECK(!sampling_thread_.get());
}

void V8SamplingProfiler::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"), &enabled);
  if (!enabled)
    return;

  // Do not enable sampling profiler in continuous mode, as losing
  // Jit code events may not be afforded.
  base::trace_event::TraceRecordMode record_mode =
      TraceLog::GetInstance()->GetCurrentTraceOptions().record_mode;
  if (record_mode == base::trace_event::TraceRecordMode::RECORD_CONTINUOUSLY)
    return;

  DCHECK(!sampling_thread_.get());
  sampling_thread_.reset(new V8SamplingThread(
      render_thread_sampler_.get(), waitable_event_for_testing_.get()));
  sampling_thread_->Start();
}

void V8SamplingProfiler::OnTraceLogDisabled() {
  if (!sampling_thread_.get())
    return;
  sampling_thread_->Stop();
  sampling_thread_.reset();
}

void V8SamplingProfiler::EnableSamplingEventForTesting() {
  waitable_event_for_testing_.reset(new base::WaitableEvent(false, false));
}

void V8SamplingProfiler::WaitSamplingEventForTesting() {
  waitable_event_for_testing_->Wait();
}

}  // namespace blink
