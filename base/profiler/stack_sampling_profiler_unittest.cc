// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <intrin.h>
#include <malloc.h>
#else
#include <alloca.h>
#endif

// STACK_SAMPLING_PROFILER_SUPPORTED is used to conditionally enable the tests
// below for supported platforms (currently Win x64).
#if defined(_WIN64)
#define STACK_SAMPLING_PROFILER_SUPPORTED 1
#endif

#if defined(OS_WIN)
#pragma intrinsic(_ReturnAddress)
#endif

namespace base {

using SamplingParams = StackSamplingProfiler::SamplingParams;
using Frame = StackSamplingProfiler::Frame;
using Module = StackSamplingProfiler::Module;
using Sample = StackSamplingProfiler::Sample;
using CallStackProfile = StackSamplingProfiler::CallStackProfile;
using CallStackProfiles = StackSamplingProfiler::CallStackProfiles;

namespace {

// Configuration for whether to allocate dynamic stack memory.
enum DynamicStackAllocationConfig { USE_ALLOCA, NO_ALLOCA };

// Signature for a target function that is expected to appear in the stack. See
// SignalAndWaitUntilSignaled() below. The return value should be a program
// counter pointer near the end of the function.
using TargetFunction = const void*(*)(WaitableEvent*, WaitableEvent*);

// A thread to target for profiling, whose stack is guaranteed to contain
// SignalAndWaitUntilSignaled() when coordinated with the main thread.
class TargetThread : public PlatformThread::Delegate {
 public:
  TargetThread(DynamicStackAllocationConfig allocation_config);

  // PlatformThread::Delegate:
  void ThreadMain() override;

  // Waits for the thread to have started and be executing in
  // SignalAndWaitUntilSignaled().
  void WaitForThreadStart();

  // Allows the thread to return from SignalAndWaitUntilSignaled() and finish
  // execution.
  void SignalThreadToFinish();

  // This function is guaranteed to be executing between calls to
  // WaitForThreadStart() and SignalThreadToFinish() when invoked with
  // |thread_started_event_| and |finish_event_|. Returns a program counter
  // value near the end of the function. May be invoked with null WaitableEvents
  // to just return the program counter.
  //
  // This function is static so that we can get a straightforward address
  // for it in one of the tests below, rather than dealing with the complexity
  // of a member function pointer representation.
  static const void* SignalAndWaitUntilSignaled(
      WaitableEvent* thread_started_event,
      WaitableEvent* finish_event);

  // Works like SignalAndWaitUntilSignaled() but additionally allocates memory
  // on the stack with alloca. Note that this must be a separate function from
  // SignalAndWaitUntilSignaled because on Windows x64 the compiler sets up
  // dynamic frame handling whenever alloca appears in a function, even if only
  // conditionally invoked.
  static const void* SignalAndWaitUntilSignaledWithAlloca(
      WaitableEvent* thread_started_event,
      WaitableEvent* finish_event);

  PlatformThreadId id() const { return id_; }

 private:
  // Returns the current program counter, or a value very close to it.
  static const void* GetProgramCounter();

  WaitableEvent thread_started_event_;
  WaitableEvent finish_event_;
  PlatformThreadId id_;
  const DynamicStackAllocationConfig allocation_config_;

  DISALLOW_COPY_AND_ASSIGN(TargetThread);
};

TargetThread::TargetThread(DynamicStackAllocationConfig allocation_config)
    : thread_started_event_(false, false), finish_event_(false, false),
      id_(0), allocation_config_(allocation_config) {}

void TargetThread::ThreadMain() {
  id_ = PlatformThread::CurrentId();
  if (allocation_config_ == USE_ALLOCA) {
    SignalAndWaitUntilSignaledWithAlloca(&thread_started_event_,
                                         &finish_event_);
  } else {
    SignalAndWaitUntilSignaled(&thread_started_event_, &finish_event_);
  }
}

void TargetThread::WaitForThreadStart() {
  thread_started_event_.Wait();
}

void TargetThread::SignalThreadToFinish() {
  finish_event_.Signal();
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE const void* TargetThread::SignalAndWaitUntilSignaled(
    WaitableEvent* thread_started_event,
    WaitableEvent* finish_event) {
  if (thread_started_event && finish_event) {
    thread_started_event->Signal();
    finish_event->Wait();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile program_counter = GetProgramCounter();
  return program_counter;
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE const void* TargetThread::SignalAndWaitUntilSignaledWithAlloca(
    WaitableEvent* thread_started_event,
    WaitableEvent* finish_event) {
  const size_t alloca_size = 100;
  // Memset to 0 to generate a clean failure.
  std::memset(alloca(alloca_size), 0, alloca_size);

  if (thread_started_event && finish_event) {
    thread_started_event->Signal();
    finish_event->Wait();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile program_counter = GetProgramCounter();
  return program_counter;
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE const void* TargetThread::GetProgramCounter() {
#if defined(OS_WIN)
  return _ReturnAddress();
#else
  return __builtin_return_address(0);
#endif
}


// Called on the profiler thread when complete, to collect profiles.
void SaveProfiles(CallStackProfiles* profiles,
                  const CallStackProfiles& pending_profiles) {
  *profiles = pending_profiles;
}

// Called on the profiler thread when complete. Collects profiles produced by
// the profiler, and signals an event to allow the main thread to know that that
// the profiler is done.
void SaveProfilesAndSignalEvent(CallStackProfiles* profiles,
                                WaitableEvent* event,
                                const CallStackProfiles& pending_profiles) {
  *profiles = pending_profiles;
  event->Signal();
}

// Executes the function with the target thread running and executing within
// SignalAndWaitUntilSignaled() or SignalAndWaitUntilSignaledWithAlloca(),
// depending on the value of |allocation_config|. Performs all necessary target
// thread startup and shutdown work before and afterward.
template <class Function>
void WithTargetThread(Function function,
                      DynamicStackAllocationConfig allocation_config) {
  TargetThread target_thread(allocation_config);
  PlatformThreadHandle target_thread_handle;
  EXPECT_TRUE(PlatformThread::Create(0, &target_thread, &target_thread_handle));

  target_thread.WaitForThreadStart();

  function(target_thread.id());

  target_thread.SignalThreadToFinish();

  PlatformThread::Join(target_thread_handle);
}

template <class Function>
void WithTargetThread(Function function) {
  WithTargetThread(function, NO_ALLOCA);
}

// Captures profiles as specified by |params| on the TargetThread, and returns
// them in |profiles|. Waits up to |profiler_wait_time| for the profiler to
// complete.
void CaptureProfiles(const SamplingParams& params, TimeDelta profiler_wait_time,
                     CallStackProfiles* profiles) {
  profiles->clear();

  WithTargetThread([&params, profiles, profiler_wait_time](
      PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(true, false);
    const StackSamplingProfiler::CompletedCallback callback =
        Bind(&SaveProfilesAndSignalEvent, Unretained(profiles),
             Unretained(&sampling_thread_completed));
    StackSamplingProfiler profiler(target_thread_id, params, callback);
    profiler.Start();
    sampling_thread_completed.TimedWait(profiler_wait_time);
    profiler.Stop();
    sampling_thread_completed.Wait();
  });
}

// If this executable was linked with /INCREMENTAL (the default for non-official
// debug and release builds on Windows), function addresses do not correspond to
// function code itself, but instead to instructions in the Incremental Link
// Table that jump to the functions. Checks for a jump instruction and if
// present does a little decompilation to find the function's actual starting
// address.
const void* MaybeFixupFunctionAddressForILT(const void* function_address) {
#if defined(_WIN64)
  const unsigned char* opcode =
      reinterpret_cast<const unsigned char*>(function_address);
  if (*opcode == 0xe9) {
    // This is a relative jump instruction. Assume we're in the ILT and compute
    // the function start address from the instruction offset.
    const int32* offset = reinterpret_cast<const int32*>(opcode + 1);
    const unsigned char* next_instruction =
        reinterpret_cast<const unsigned char*>(offset + 1);
    return next_instruction + *offset;
  }
#endif
  return function_address;
}

// Searches through the frames in |sample|, returning an iterator to the first
// frame that has an instruction pointer within |target_function|. Returns
// sample.end() if no such frames are found.
Sample::const_iterator FindFirstFrameWithinFunction(
    const Sample& sample,
    TargetFunction target_function) {
  uintptr_t function_start = reinterpret_cast<uintptr_t>(
      MaybeFixupFunctionAddressForILT(reinterpret_cast<const void*>(
          target_function)));
  uintptr_t function_end =
      reinterpret_cast<uintptr_t>(target_function(nullptr, nullptr));
  for (auto it = sample.begin(); it != sample.end(); ++it) {
    if ((it->instruction_pointer >= function_start) &&
        (it->instruction_pointer <= function_end))
      return it;
  }
  return sample.end();
}

// Formats a sample into a string that can be output for test diagnostics.
std::string FormatSampleForDiagnosticOutput(
    const Sample& sample,
    const std::vector<Module>& modules) {
  std::string output;
  for (const Frame& frame: sample) {
    output += StringPrintf(
        "0x%p %s\n", reinterpret_cast<const void*>(frame.instruction_pointer),
        modules[frame.module_index].filename.AsUTF8Unsafe().c_str());
  }
  return output;
}

// Returns a duration that is longer than the test timeout. We would use
// TimeDelta::Max() but https://crbug.com/465948.
TimeDelta AVeryLongTimeDelta() { return TimeDelta::FromDays(1); }

}  // namespace

// Checks that the basic expected information is present in a sampled call stack
// profile.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
TEST(StackSamplingProfilerTest, MAYBE_Basic) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_burst = 1;

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, AVeryLongTimeDelta(), &profiles);

  // Check that the profile and samples sizes are correct, and the module
  // indices are in range.
  ASSERT_EQ(1u, profiles.size());
  const CallStackProfile& profile = profiles[0];
  ASSERT_EQ(1u, profile.samples.size());
  EXPECT_EQ(params.sampling_interval, profile.sampling_period);
  const Sample& sample = profile.samples[0];
  for (const auto& frame : sample) {
    ASSERT_GE(frame.module_index, 0u);
    ASSERT_LT(frame.module_index, profile.modules.size());
  }

  // Check that the stack contains a frame for
  // TargetThread::SignalAndWaitUntilSignaled() and that the frame has this
  // executable's module.
  Sample::const_iterator loc = FindFirstFrameWithinFunction(
      sample,
      &TargetThread::SignalAndWaitUntilSignaled);
  ASSERT_TRUE(loc != sample.end())
      << "Function at "
      << MaybeFixupFunctionAddressForILT(reinterpret_cast<const void*>(
          &TargetThread::SignalAndWaitUntilSignaled))
      << " was not found in stack:\n"
      << FormatSampleForDiagnosticOutput(sample, profile.modules);
  FilePath executable_path;
  EXPECT_TRUE(PathService::Get(FILE_EXE, &executable_path));
  EXPECT_EQ(executable_path, profile.modules[loc->module_index].filename);
}

// Checks that the profiler handles stacks containing dynamically-allocated
// stack memory.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_Alloca Alloca
#else
#define MAYBE_Alloca DISABLED_Alloca
#endif
TEST(StackSamplingProfilerTest, MAYBE_Alloca) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_burst = 1;

  std::vector<CallStackProfile> profiles;
  WithTargetThread([&params, &profiles](
      PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(true, false);
    const StackSamplingProfiler::CompletedCallback callback =
        Bind(&SaveProfilesAndSignalEvent, Unretained(&profiles),
             Unretained(&sampling_thread_completed));
    StackSamplingProfiler profiler(target_thread_id, params, callback);
    profiler.Start();
    sampling_thread_completed.Wait();
  }, USE_ALLOCA);

  // Look up the sample.
  ASSERT_EQ(1u, profiles.size());
  const CallStackProfile& profile = profiles[0];
  ASSERT_EQ(1u, profile.samples.size());
  const Sample& sample = profile.samples[0];

  // Check that the stack contains a frame for
  // TargetThread::SignalAndWaitUntilSignaledWithAlloca().
  Sample::const_iterator loc = FindFirstFrameWithinFunction(
      sample,
      &TargetThread::SignalAndWaitUntilSignaledWithAlloca);
  ASSERT_TRUE(loc != sample.end())
      << "Function at "
      << MaybeFixupFunctionAddressForILT(reinterpret_cast<const void*>(
          &TargetThread::SignalAndWaitUntilSignaledWithAlloca))
      << " was not found in stack:\n"
      << FormatSampleForDiagnosticOutput(sample, profile.modules);
}

// Checks that the fire-and-forget interface works.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_StartAndRunAsync StartAndRunAsync
#else
#define MAYBE_StartAndRunAsync DISABLED_StartAndRunAsync
#endif
TEST(StackSamplingProfilerTest, MAYBE_StartAndRunAsync) {
  // StartAndRunAsync requires the caller to have a message loop.
  MessageLoop message_loop;

  SamplingParams params;
  params.samples_per_burst = 1;

  CallStackProfiles profiles;
  WithTargetThread([&params, &profiles](PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(false, false);
    const StackSamplingProfiler::CompletedCallback callback =
        Bind(&SaveProfilesAndSignalEvent, Unretained(&profiles),
             Unretained(&sampling_thread_completed));
    StackSamplingProfiler::StartAndRunAsync(target_thread_id, params, callback);
    RunLoop().RunUntilIdle();
    sampling_thread_completed.Wait();
  });

  ASSERT_EQ(1u, profiles.size());
}

// Checks that the expected number of profiles and samples are present in the
// call stack profiles produced.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_MultipleProfilesAndSamples MultipleProfilesAndSamples
#else
#define MAYBE_MultipleProfilesAndSamples DISABLED_MultipleProfilesAndSamples
#endif
TEST(StackSamplingProfilerTest, MAYBE_MultipleProfilesAndSamples) {
  SamplingParams params;
  params.burst_interval = params.sampling_interval =
      TimeDelta::FromMilliseconds(0);
  params.bursts = 2;
  params.samples_per_burst = 3;

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, AVeryLongTimeDelta(), &profiles);

  ASSERT_EQ(2u, profiles.size());
  EXPECT_EQ(3u, profiles[0].samples.size());
  EXPECT_EQ(3u, profiles[1].samples.size());
}

// Checks that no call stack profiles are captured if the profiling is stopped
// during the initial delay.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_StopDuringInitialDelay StopDuringInitialDelay
#else
#define MAYBE_StopDuringInitialDelay DISABLED_StopDuringInitialDelay
#endif
TEST(StackSamplingProfilerTest, MAYBE_StopDuringInitialDelay) {
  SamplingParams params;
  params.initial_delay = TimeDelta::FromSeconds(60);

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, TimeDelta::FromMilliseconds(0), &profiles);

  EXPECT_TRUE(profiles.empty());
}

// Checks that the single completed call stack profile is captured if the
// profiling is stopped between bursts.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_StopDuringInterBurstInterval StopDuringInterBurstInterval
#else
#define MAYBE_StopDuringInterBurstInterval DISABLED_StopDuringInterBurstInterval
#endif
TEST(StackSamplingProfilerTest, MAYBE_StopDuringInterBurstInterval) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.burst_interval = TimeDelta::FromSeconds(60);
  params.bursts = 2;
  params.samples_per_burst = 1;

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, TimeDelta::FromMilliseconds(50), &profiles);

  ASSERT_EQ(1u, profiles.size());
  EXPECT_EQ(1u, profiles[0].samples.size());
}

// Checks that incomplete call stack profiles are captured.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_StopDuringInterSampleInterval StopDuringInterSampleInterval
#else
#define MAYBE_StopDuringInterSampleInterval \
  DISABLED_StopDuringInterSampleInterval
#endif
TEST(StackSamplingProfilerTest, MAYBE_StopDuringInterSampleInterval) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromSeconds(60);
  params.samples_per_burst = 2;

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, TimeDelta::FromMilliseconds(50), &profiles);

  ASSERT_EQ(1u, profiles.size());
  EXPECT_EQ(1u, profiles[0].samples.size());
}

// Checks that we can destroy the profiler while profiling.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_DestroyProfilerWhileProfiling DestroyProfilerWhileProfiling
#else
#define MAYBE_DestroyProfilerWhileProfiling \
  DISABLED_DestroyProfilerWhileProfiling
#endif
TEST(StackSamplingProfilerTest, MAYBE_DestroyProfilerWhileProfiling) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(10);

  CallStackProfiles profiles;
  WithTargetThread([&params, &profiles](PlatformThreadId target_thread_id) {
    scoped_ptr<StackSamplingProfiler> profiler;
    profiler.reset(new StackSamplingProfiler(
        target_thread_id, params, Bind(&SaveProfiles, Unretained(&profiles))));
    profiler->Start();
    profiler.reset();

    // Wait longer than a sample interval to catch any use-after-free actions by
    // the profiler thread.
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(50));
  });
}

// Checks that the same profiler may be run multiple times.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_CanRunMultipleTimes CanRunMultipleTimes
#else
#define MAYBE_CanRunMultipleTimes DISABLED_CanRunMultipleTimes
#endif
TEST(StackSamplingProfilerTest, MAYBE_CanRunMultipleTimes) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_burst = 1;

  std::vector<CallStackProfile> profiles;
  CaptureProfiles(params, AVeryLongTimeDelta(), &profiles);
  ASSERT_EQ(1u, profiles.size());

  profiles.clear();
  CaptureProfiles(params, AVeryLongTimeDelta(), &profiles);
  ASSERT_EQ(1u, profiles.size());
}

// Checks that requests to start profiling while another profile is taking place
// are ignored.
#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define MAYBE_ConcurrentProfiling ConcurrentProfiling
#else
#define MAYBE_ConcurrentProfiling DISABLED_ConcurrentProfiling
#endif
TEST(StackSamplingProfilerTest, MAYBE_ConcurrentProfiling) {
  WithTargetThread([](PlatformThreadId target_thread_id) {
    SamplingParams params[2];
    params[0].initial_delay = TimeDelta::FromMilliseconds(10);
    params[0].sampling_interval = TimeDelta::FromMilliseconds(0);
    params[0].samples_per_burst = 1;

    params[1].sampling_interval = TimeDelta::FromMilliseconds(0);
    params[1].samples_per_burst = 1;

    CallStackProfiles profiles[2];
    ScopedVector<WaitableEvent> sampling_completed;
    ScopedVector<StackSamplingProfiler> profiler;
    for (int i = 0; i < 2; ++i) {
      sampling_completed.push_back(new WaitableEvent(false, false));
      const StackSamplingProfiler::CompletedCallback callback =
          Bind(&SaveProfilesAndSignalEvent, Unretained(&profiles[i]),
               Unretained(sampling_completed[i]));
      profiler.push_back(
          new StackSamplingProfiler(target_thread_id, params[i], callback));
    }

    profiler[0]->Start();
    profiler[1]->Start();

    // Wait for one profiler to finish.
    size_t completed_profiler =
        WaitableEvent::WaitMany(&sampling_completed[0], 2);
    EXPECT_EQ(1u, profiles[completed_profiler].size());

    size_t other_profiler = 1 - completed_profiler;
    // Give the other profiler a chance to run and observe that it hasn't.
    EXPECT_FALSE(sampling_completed[other_profiler]->TimedWait(
        TimeDelta::FromMilliseconds(25)));

    // Start the other profiler again and it should run.
    profiler[other_profiler]->Start();
    sampling_completed[other_profiler]->Wait();
    EXPECT_EQ(1u, profiles[other_profiler].size());
  });
}

}  // namespace base
