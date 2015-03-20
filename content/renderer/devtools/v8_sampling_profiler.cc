// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools/v8_sampling_profiler.h"

#if defined(OS_POSIX)
#include <signal.h>
#define USE_SIGNALS
#endif

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "content/renderer/devtools/lock_free_circular_queue.h"
#include "content/renderer/render_thread_impl.h"
#include "v8/include/v8.h"

using base::trace_event::ConvertableToTraceFormat;
using base::trace_event::TraceLog;
using base::trace_event::TracedValue;
using v8::Isolate;

namespace content {

namespace {

std::string PtrToString(const void* value) {
  return base::StringPrintf(
      "0x%" PRIx64, static_cast<uint64>(reinterpret_cast<intptr_t>(value)));
}

class SampleRecord {
 public:
  static const int kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1u << kMaxFramesCountLog2) - 1;

  SampleRecord() {}

  base::TimeTicks timestamp() const { return timestamp_; }
  void Collect(v8::Isolate* isolate,
               base::TimeTicks timestamp,
               const v8::RegisterState& state);
  scoped_refptr<ConvertableToTraceFormat> ToTraceFormat() const;

 private:
  base::TimeTicks timestamp_;
  unsigned vm_state_ : 4;
  unsigned frames_count_ : kMaxFramesCountLog2;
  const void* frames_[kMaxFramesCount];

  DISALLOW_COPY_AND_ASSIGN(SampleRecord);
};

void SampleRecord::Collect(v8::Isolate* isolate,
                           base::TimeTicks timestamp,
                           const v8::RegisterState& state) {
  v8::SampleInfo sample_info;
  isolate->GetStackSample(state, (void**)frames_, kMaxFramesCount,
                          &sample_info);
  timestamp_ = timestamp;
  frames_count_ = sample_info.frames_count;
  vm_state_ = sample_info.vm_state;
}

scoped_refptr<ConvertableToTraceFormat> SampleRecord::ToTraceFormat() const {
  scoped_refptr<TracedValue> data(new TracedValue());
  const char* vm_state = nullptr;
  switch (vm_state_) {
    case v8::StateTag::JS:
      vm_state = "js";
      break;
    case v8::StateTag::GC:
      vm_state = "gc";
      break;
    case v8::StateTag::COMPILER:
      vm_state = "compiler";
      break;
    case v8::StateTag::OTHER:
      vm_state = "other";
      break;
    case v8::StateTag::EXTERNAL:
      vm_state = "external";
      break;
    case v8::StateTag::IDLE:
      vm_state = "idle";
      break;
    default:
      NOTREACHED();
  }
  data->SetString("vm_state", vm_state);
  data->BeginArray("stack");
  for (unsigned i = 0; i < frames_count_; ++i) {
    data->AppendString(PtrToString(frames_[i]));
  }
  data->EndArray();
  return data;
}

}  // namespace

// The class implements a sampler responsible for sampling a single thread.
class Sampler {
 public:
  ~Sampler();

  static scoped_ptr<Sampler> CreateForCurrentThread();
  static Sampler* GetInstance() { return tls_instance_.Pointer()->Get(); }

  // These methods are called from the sampling thread.
  void Start();
  void Stop();
  void Sample();

  void DoSample(const v8::RegisterState& state);

  bool EventsCollectedForTest() const {
    return base::subtle::NoBarrier_Load(&code_added_events_count_) != 0 ||
           base::subtle::NoBarrier_Load(&samples_count_) != 0;
  }

 private:
  Sampler();

  static void InstallJitCodeEventHandler(Isolate* isolate, void* data);
  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);
  static scoped_refptr<ConvertableToTraceFormat> JitCodeEventToTraceFormat(
      const v8::JitCodeEvent* event);
  static base::PlatformThreadHandle GetCurrentThreadHandle();

  void InjectPendingEvents();

  static const unsigned kNumberOfSamples = 10;
  typedef LockFreeCircularQueue<SampleRecord, kNumberOfSamples> SamplingQueue;

  base::PlatformThreadId thread_id_;
  base::PlatformThreadHandle thread_handle_;
  Isolate* isolate_;
  scoped_ptr<SamplingQueue> samples_data_;
  base::subtle::Atomic32 code_added_events_count_;
  base::subtle::Atomic32 samples_count_;

  static base::LazyInstance<base::ThreadLocalPointer<Sampler>>::Leaky
      tls_instance_;
};

base::LazyInstance<base::ThreadLocalPointer<Sampler>>::Leaky
    Sampler::tls_instance_ = LAZY_INSTANCE_INITIALIZER;

Sampler::Sampler()
    : thread_id_(base::PlatformThread::CurrentId()),
      thread_handle_(Sampler::GetCurrentThreadHandle()),
      isolate_(Isolate::GetCurrent()),
      code_added_events_count_(0),
      samples_count_(0) {
  DCHECK(isolate_);
  DCHECK(!GetInstance());
  tls_instance_.Pointer()->Set(this);
}

Sampler::~Sampler() {
  DCHECK(GetInstance());
  tls_instance_.Pointer()->Set(nullptr);
}

// static
scoped_ptr<Sampler> Sampler::CreateForCurrentThread() {
  return scoped_ptr<Sampler>(new Sampler());
}

// static
base::PlatformThreadHandle Sampler::GetCurrentThreadHandle() {
#ifdef OS_WIN
  // TODO(alph): Add Windows support.
  return base::PlatformThreadHandle();
#else
  return base::PlatformThread::CurrentHandle();
#endif
}

void Sampler::Start() {
  samples_data_.reset(new SamplingQueue());
  v8::JitCodeEventHandler handler = &HandleJitCodeEvent;
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler,
                             reinterpret_cast<void*>(handler));
}

void Sampler::Stop() {
  isolate_->RequestInterrupt(&InstallJitCodeEventHandler, nullptr);
  samples_data_.reset();
}

void Sampler::Sample() {
#if defined(USE_SIGNALS)
  int error = pthread_kill(thread_handle_.platform_handle(), SIGPROF);
  if (error) {
    LOG(ERROR) << "pthread_kill failed with error " << error << " "
               << strerror(error);
  }
  InjectPendingEvents();
#endif
}

void Sampler::DoSample(const v8::RegisterState& state) {
  // Called in the sampled thread signal handler.
  // Because of that it is not allowed to do any memory allocation here.
  base::TimeTicks timestamp = base::TimeTicks::NowFromSystemTraceTime();
  SampleRecord* record = samples_data_->StartEnqueue();
  if (!record)
    return;
  record->Collect(isolate_, timestamp, state);
  samples_data_->FinishEnqueue();
  base::subtle::NoBarrier_AtomicIncrement(&samples_count_, 1);
}

void Sampler::InjectPendingEvents() {
  SampleRecord* record = samples_data_->Peek();
  while (record) {
    TRACE_EVENT_SAMPLE_WITH_TID_AND_TIMESTAMP1(
        TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"), "V8Sample", thread_id_,
        (record->timestamp() - base::TimeTicks()).InMicroseconds(), "data",
        record->ToTraceFormat());
    samples_data_->Remove();
    record = samples_data_->Peek();
  }
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
  Sampler* sampler = GetInstance();
  // The sampler may have already been destroyed.
  // That's fine, we're not interested in these events anymore.
  if (!sampler)
    return;
  switch (event->type) {
    case v8::JitCodeEvent::CODE_ADDED:
      TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"),
                           "JitCodeAdded", TRACE_EVENT_SCOPE_THREAD, "data",
                           JitCodeEventToTraceFormat(event));
      base::subtle::NoBarrier_AtomicIncrement(
          &sampler->code_added_events_count_, 1);
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
scoped_refptr<ConvertableToTraceFormat> Sampler::JitCodeEventToTraceFormat(
    const v8::JitCodeEvent* event) {
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

  static void InstallSignalHandler();
  static void RestoreSignalHandler();
#ifdef USE_SIGNALS
  static void HandleProfilerSignal(int signal, siginfo_t* info, void* context);
#endif

  static void HandleJitCodeEvent(const v8::JitCodeEvent* event);

  Sampler* render_thread_sampler_;
  base::CancellationFlag cancellation_flag_;
  base::WaitableEvent* waitable_event_for_testing_;
  base::PlatformThreadHandle sampling_thread_handle_;
  std::vector<Sampler*> samplers_;

#ifdef USE_SIGNALS
  static bool signal_handler_installed_;
  static struct sigaction old_signal_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(V8SamplingThread);
};

#ifdef USE_SIGNALS
bool V8SamplingThread::signal_handler_installed_;
struct sigaction V8SamplingThread::old_signal_handler_;
#endif

V8SamplingThread::V8SamplingThread(Sampler* render_thread_sampler,
                                   base::WaitableEvent* event)
    : render_thread_sampler_(render_thread_sampler),
      waitable_event_for_testing_(event) {
}

void V8SamplingThread::ThreadMain() {
  base::PlatformThread::SetName("V8SamplingProfilerThread");
  InstallSamplers();
  StartSamplers();
  InstallSignalHandler();
  const int kSamplingFrequencyMicroseconds = 1000;
  while (!cancellation_flag_.IsSet()) {
    Sample();
    if (waitable_event_for_testing_ &&
        render_thread_sampler_->EventsCollectedForTest()) {
      waitable_event_for_testing_->Signal();
    }
    // TODO(alph): make the samples firing interval not depend on the sample
    // taking duration.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMicroseconds(kSamplingFrequencyMicroseconds));
  }
  RestoreSignalHandler();
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

// static
void V8SamplingThread::InstallSignalHandler() {
#ifdef USE_SIGNALS
  // There must be the only one!
  DCHECK(!signal_handler_installed_);
  struct sigaction sa;
  sa.sa_sigaction = &HandleProfilerSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  signal_handler_installed_ =
      (sigaction(SIGPROF, &sa, &old_signal_handler_) == 0);
#endif
}

// static
void V8SamplingThread::RestoreSignalHandler() {
#ifdef USE_SIGNALS
  if (!signal_handler_installed_)
    return;
  sigaction(SIGPROF, &old_signal_handler_, 0);
  signal_handler_installed_ = false;
#endif
}

#ifdef USE_SIGNALS
// static
void V8SamplingThread::HandleProfilerSignal(int signal,
                                            siginfo_t* info,
                                            void* context) {
  if (signal != SIGPROF)
    return;
  ucontext_t* ucontext = reinterpret_cast<ucontext_t*>(context);
  mcontext_t& mcontext = ucontext->uc_mcontext;
  v8::RegisterState state;

#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
  // TODO(alph): Add support for Android and ChromeOS
  ALLOW_UNUSED_LOCAL(mcontext);

#elif defined(OS_MACOSX)
#if ARCH_CPU_64_BITS
  state.pc = reinterpret_cast<void*>(mcontext->__ss.__rip);
  state.sp = reinterpret_cast<void*>(mcontext->__ss.__rsp);
  state.fp = reinterpret_cast<void*>(mcontext->__ss.__rbp);
#elif ARCH_CPU_32_BITS
  state.pc = reinterpret_cast<void*>(mcontext->__ss.__eip);
  state.sp = reinterpret_cast<void*>(mcontext->__ss.__esp);
  state.fp = reinterpret_cast<void*>(mcontext->__ss.__ebp);
#endif  // ARCH_CPU_32_BITS

#else
#if ARCH_CPU_64_BITS
  state.pc = reinterpret_cast<void*>(mcontext.gregs[REG_RIP]);
  state.sp = reinterpret_cast<void*>(mcontext.gregs[REG_RSP]);
  state.fp = reinterpret_cast<void*>(mcontext.gregs[REG_RBP]);
#elif ARCH_CPU_32_BITS
  state.pc = reinterpret_cast<void*>(mcontext.gregs[REG_EIP]);
  state.sp = reinterpret_cast<void*>(mcontext.gregs[REG_ESP]);
  state.fp = reinterpret_cast<void*>(mcontext.gregs[REG_EBP]);
#endif  // ARCH_CPU_32_BITS
#endif
  Sampler::GetInstance()->DoSample(state);
}
#endif

void V8SamplingThread::Start() {
  if (!base::PlatformThread::Create(0, this, &sampling_thread_handle_)) {
    DCHECK(false) << "failed to create sampling thread";
  }
}

void V8SamplingThread::Stop() {
  cancellation_flag_.Set();
  base::PlatformThread::Join(sampling_thread_handle_);
}

V8SamplingProfiler::V8SamplingProfiler(bool underTest)
    : sampling_thread_(nullptr),
      render_thread_sampler_(Sampler::CreateForCurrentThread()),
      message_loop_proxy_(base::MessageLoopProxy::current()) {
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

void V8SamplingProfiler::StartSamplingThread() {
  DCHECK(!sampling_thread_.get());
  sampling_thread_.reset(new V8SamplingThread(
      render_thread_sampler_.get(), waitable_event_for_testing_.get()));
  sampling_thread_->Start();
}

void V8SamplingProfiler::OnTraceLogEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("v8.cpu_profile"), &enabled);
  if (!enabled)
    return;

  // Do not enable sampling profiler in continuous mode, as losing
  // Jit code events may not be afforded.
  // TODO(alph): add support of infinite recording of meta trace events.
  base::trace_event::TraceRecordMode record_mode =
      TraceLog::GetInstance()->GetCurrentTraceOptions().record_mode;
  if (record_mode == base::trace_event::TraceRecordMode::RECORD_CONTINUOUSLY)
    return;

  message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&V8SamplingProfiler::StartSamplingThread,
                            base::Unretained(this)));
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
