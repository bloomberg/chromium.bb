// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include <intrin.h>
#include <malloc.h>
#include <windows.h>
#else
#include <alloca.h>
#endif

// STACK_SAMPLING_PROFILER_SUPPORTED is used to conditionally enable the tests
// below for supported platforms (currently Win x64 and Mac x64).
#if defined(_WIN64) || (defined(OS_MACOSX) && !defined(OS_IOS))
#define STACK_SAMPLING_PROFILER_SUPPORTED 1
#endif

#if defined(OS_WIN)
#pragma intrinsic(_ReturnAddress)
#endif

namespace base {

#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define PROFILER_TEST_F(TestClass, TestName) TEST_F(TestClass, TestName)
#else
#define PROFILER_TEST_F(TestClass, TestName) \
  TEST_F(TestClass, DISABLED_##TestName)
#endif

using SamplingParams = StackSamplingProfiler::SamplingParams;
using Frames = std::vector<Frame>;
using FrameSets = std::vector<std::vector<Frame>>;

namespace {

// Addresses near the start and end of a function.
struct FunctionAddressRange {
  const void* start;
  const void* end;
};

// Disable inlining for this function so that it gets its own stack frame.
NOINLINE const void* GetProgramCounter() {
  // Saving to a volatile prevents clang from assuming it can reuse the return
  // value across multiple calls to GetProgramCounter().
#if defined(OS_WIN)
  const void* volatile program_counter = _ReturnAddress();
#else
  const void* volatile program_counter = __builtin_return_address(0);
#endif
  return program_counter;
}

// Represents a stack unwind scenario to be sampled by the
// StackSamplingProfiler.
class UnwindScenario {
 public:
  // A callback provided by the caller that sets up the unwind scenario, then
  // calls into the passed closure to wait for a sample to be taken. Returns the
  // address range of the function that sets up the unwind scenario. The passed
  // closure will be null when invoked solely to obtain the address range.
  using SetupFunction = RepeatingCallback<FunctionAddressRange(const Closure&)>;

  // Events to coordinate the sampling.
  struct SampleEvents {
    WaitableEvent ready_for_sample;
    WaitableEvent sample_finished;
  };

  explicit UnwindScenario(const SetupFunction& setup_function)
      : setup_function_(setup_function) {}

  UnwindScenario(const UnwindScenario&) = delete;
  UnwindScenario& operator=(const UnwindScenario&) = delete;

  FunctionAddressRange GetWaitForSampleAddressRange() const;
  FunctionAddressRange GetSetupFunctionAddressRange() const;
  FunctionAddressRange GetOuterFunctionAddressRange() const;

  void Execute(SampleEvents* events);

 private:
  static FunctionAddressRange InvokeSetupFunction(
      const SetupFunction& setup_function,
      SampleEvents* events);

  static FunctionAddressRange WaitForSample(SampleEvents* events);

  const SetupFunction setup_function_;
};

FunctionAddressRange UnwindScenario::GetWaitForSampleAddressRange() const {
  return WaitForSample(nullptr);
}

FunctionAddressRange UnwindScenario::GetSetupFunctionAddressRange() const {
  return setup_function_.Run(Closure());
}

FunctionAddressRange UnwindScenario::GetOuterFunctionAddressRange() const {
  return InvokeSetupFunction(SetupFunction(), nullptr);
}

void UnwindScenario::Execute(SampleEvents* events) {
  InvokeSetupFunction(setup_function_, events);
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
UnwindScenario::InvokeSetupFunction(const SetupFunction& setup_function,
                                    SampleEvents* events) {
  const void* start_program_counter = GetProgramCounter();

  if (!setup_function.is_null()) {
    const auto wait_for_sample_closure =
        BindLambdaForTesting([&]() { UnwindScenario::WaitForSample(events); });
    setup_function.Run(wait_for_sample_closure);
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
UnwindScenario::WaitForSample(SampleEvents* events) {
  const void* start_program_counter = GetProgramCounter();

  if (events) {
    events->ready_for_sample.Signal();
    events->sample_finished.Wait();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// A thread to target for profiling, whose stack is guaranteed to contain
// SignalAndWaitUntilSignaled() when coordinated with the main thread.
class TargetThread : public PlatformThread::Delegate {
 public:
  TargetThread(const Closure& to_run);

  // PlatformThread::Delegate:
  void ThreadMain() override;

  PlatformThreadId id() const { return id_; }

 private:
  PlatformThreadId id_ = 0;
  Closure to_run_;

  DISALLOW_COPY_AND_ASSIGN(TargetThread);
};

TargetThread::TargetThread(const Closure& to_run) : to_run_(to_run) {}

void TargetThread::ThreadMain() {
  id_ = PlatformThread::CurrentId();
  to_run_.Run();
}

// Calls into |wait_for_sample| without doing any special unwinding setup, to
// test the "normal" unwind scenario.
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
CallWithPlainFunction(const Closure& wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  if (!wait_for_sample.is_null())
    wait_for_sample.Run();

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// Calls into |wait_for_sample| after using alloca(), to test unwinding with a
// frame pointer.
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange CallWithAlloca(const Closure& wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  // Volatile to force a dynamic stack allocation.
  const volatile size_t alloca_size = 100;
  // Use the memory via volatile writes to prevent the allocation from being
  // optimized out.
  volatile char* const allocation =
      const_cast<volatile char*>(static_cast<char*>(alloca(alloca_size)));
  for (volatile char* p = allocation; p < allocation + alloca_size; ++p)
    *p = '\0';

  if (!wait_for_sample.is_null())
    wait_for_sample.Run();

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// The function to be executed by the code in the other library.
void OtherLibraryCallback(void* arg) {
  const Closure* wait_for_sample = static_cast<const Closure*>(arg);

  wait_for_sample->Run();

  // Prevent tail call.
  volatile int i = 0;
  ALLOW_UNUSED_LOCAL(i);
}

// Calls into |wait_for_sample| through a function within another library, to
// test unwinding through multiple modules and scenarios involving unloaded
// modules.
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
CallThroughOtherLibrary(NativeLibrary library, const Closure& wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  if (!wait_for_sample.is_null()) {
    // A function whose arguments are a function accepting void*, and a void*.
    using InvokeCallbackFunction = void (*)(void (*)(void*), void*);
    EXPECT_TRUE(library);
    InvokeCallbackFunction function = reinterpret_cast<InvokeCallbackFunction>(
        GetFunctionPointerFromNativeLibrary(library, "InvokeCallbackFunction"));
    EXPECT_TRUE(function);

    // This copy avoids the need for a const_cast.
    Closure wait_for_sample_copy = wait_for_sample;
    (*function)(&OtherLibraryCallback, &wait_for_sample_copy);
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// Formats a sample into a string that can be output for test diagnostics.
std::string FormatSampleForDiagnosticOutput(const Frames& frames) {
  std::string output;
  for (const auto& frame : frames) {
    output += StringPrintf(
        "0x%p %s\n", reinterpret_cast<const void*>(frame.instruction_pointer),
        frame.module ? frame.module->GetDebugBasename().AsUTF8Unsafe().c_str()
                     : "null module");
  }
  return output;
}

// Expects that the stack contains the functions with the specified address
// ranges, in the specified order.
void ExpectStackContains(const Frames& stack,
                         const std::vector<FunctionAddressRange>& functions) {
  auto frame_it = stack.begin();
  auto function_it = functions.begin();
  for (; frame_it != stack.end() && function_it != functions.end();
       ++frame_it) {
    if (frame_it->instruction_pointer >=
            reinterpret_cast<uintptr_t>(function_it->start) &&
        frame_it->instruction_pointer <=
            reinterpret_cast<uintptr_t>(function_it->end)) {
      ++function_it;
    }
  }

  EXPECT_EQ(function_it, functions.end())
      << "Function in position " << function_it - functions.begin() << " at "
      << function_it->start << " was not found in stack "
      << "(or did not appear in the expected order):\n"
      << FormatSampleForDiagnosticOutput(stack);
}

// Expects that the stack does not contain the functions with the specified
// address ranges.
void ExpectStackDoesNotContain(
    const Frames& stack,
    const std::vector<FunctionAddressRange>& functions) {
  struct FunctionAddressRangeCompare {
    bool operator()(const FunctionAddressRange& a,
                    const FunctionAddressRange& b) const {
      return std::make_pair(a.start, a.end) < std::make_pair(b.start, b.end);
    }
  };

  std::set<FunctionAddressRange, FunctionAddressRangeCompare> seen_functions;
  for (const auto frame : stack) {
    for (const auto function : functions) {
      if (frame.instruction_pointer >=
              reinterpret_cast<uintptr_t>(function.start) &&
          frame.instruction_pointer <=
              reinterpret_cast<uintptr_t>(function.end)) {
        seen_functions.insert(function);
      }
    }
  }

  for (const auto function : seen_functions) {
    ADD_FAILURE() << "Function at " << function.start
                  << " was unexpectedly found in stack:\n"
                  << FormatSampleForDiagnosticOutput(stack);
  }
}

// Profile consists of a set of frame sets and other sampling information.
struct Profile {
  Profile() = default;
  Profile(Profile&& other) = default;
  Profile(const FrameSets& frame_sets,
          int metadata_count,
          TimeDelta profile_duration,
          TimeDelta sampling_period);

  ~Profile() = default;

  Profile& operator=(Profile&& other) = default;

  // The collected frame sets.
  FrameSets frame_sets;

  // The number of invocations of RecordMetadata().
  int metadata_count;

  // Duration of this profile.
  TimeDelta profile_duration;

  // Time between samples.
  TimeDelta sampling_period;
};

Profile::Profile(const FrameSets& frame_sets,
                 int metadata_count,
                 TimeDelta profile_duration,
                 TimeDelta sampling_period)
    : frame_sets(frame_sets),
      metadata_count(metadata_count),
      profile_duration(profile_duration),
      sampling_period(sampling_period) {}

// The callback type used to collect a profile. The passed Profile is move-only.
// Other threads, including the UI thread, may block on callback completion so
// this should run as quickly as possible.
using ProfileCompletedCallback = Callback<void(Profile)>;

// TestProfileBuilder collects frames produced by the profiler.
class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder(ModuleCache* module_cache,
                     const ProfileCompletedCallback& callback);

  ~TestProfileBuilder() override;

  // ProfileBuilder:
  ModuleCache* GetModuleCache() override;
  void RecordMetadata() override;
  void OnSampleCompleted(Frames frames) override;
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override;

 private:
  ModuleCache* module_cache_;

  // The sets of frames recorded.
  std::vector<Frames> frame_sets_;

  // The number of invocations of RecordMetadata().
  int metadata_count_ = 0;

  // Callback made when sampling a profile completes.
  const ProfileCompletedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestProfileBuilder);
};

TestProfileBuilder::TestProfileBuilder(ModuleCache* module_cache,
                                       const ProfileCompletedCallback& callback)
    : module_cache_(module_cache), callback_(callback) {}

TestProfileBuilder::~TestProfileBuilder() = default;

ModuleCache* TestProfileBuilder::GetModuleCache() {
  return module_cache_;
}

void TestProfileBuilder::RecordMetadata() {
  ++metadata_count_;
}

void TestProfileBuilder::OnSampleCompleted(Frames frames) {
  frame_sets_.push_back(std::move(frames));
}

void TestProfileBuilder::OnProfileCompleted(TimeDelta profile_duration,
                                            TimeDelta sampling_period) {
  callback_.Run(
      Profile(frame_sets_, metadata_count_, profile_duration, sampling_period));
}

// Loads the other library, which defines a function to be called in the
// WITH_OTHER_LIBRARY configuration.
NativeLibrary LoadOtherLibrary() {
  // The lambda gymnastics works around the fact that we can't use ASSERT_*
  // macros in a function returning non-null.
  const auto load = [](NativeLibrary* library) {
    FilePath other_library_path;
    ASSERT_TRUE(PathService::Get(DIR_EXE, &other_library_path));
    other_library_path = other_library_path.AppendASCII(
        GetNativeLibraryName("base_profiler_test_support_library"));
    NativeLibraryLoadError load_error;
    *library = LoadNativeLibrary(other_library_path, &load_error);
    ASSERT_TRUE(*library) << "error loading " << other_library_path.value()
                          << ": " << load_error.ToString();
  };

  NativeLibrary library = nullptr;
  load(&library);
  return library;
}

// Unloads |library| and returns when it has completed unloading. Unloading a
// library is asynchronous on Windows, so simply calling UnloadNativeLibrary()
// is insufficient to ensure it's been unloaded.
void SynchronousUnloadNativeLibrary(NativeLibrary library) {
  UnloadNativeLibrary(library);
#if defined(OS_WIN)
  // NativeLibrary is a typedef for HMODULE, which is actually the base address
  // of the module.
  uintptr_t module_base_address = reinterpret_cast<uintptr_t>(library);
  HMODULE module_handle;
  // Keep trying to get the module handle until the call fails.
  while (::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCTSTR>(module_base_address),
                             &module_handle) ||
         ::GetLastError() != ERROR_MOD_NOT_FOUND) {
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
  }
#elif defined(OS_MACOSX)
// Unloading a library on the Mac is synchronous.
#else
  NOTIMPLEMENTED();
#endif
}

// Executes ProfileFunction while ThreadFunction is running on the target
// thread. Performs all necessary target thread startup and shutdown work before
// and afterward.
template <class ProfileFunction>
void WithTargetThread(UnwindScenario* scenario,
                      ProfileFunction profile_function) {
  UnwindScenario::SampleEvents events;
  TargetThread target_thread(
      BindLambdaForTesting([&]() { scenario->Execute(&events); }));

  PlatformThreadHandle target_thread_handle;
  EXPECT_TRUE(PlatformThread::Create(0, &target_thread, &target_thread_handle));

  events.ready_for_sample.Wait();

  profile_function(target_thread.id());

  events.sample_finished.Signal();

  PlatformThread::Join(target_thread_handle);
}

template <class ProfileFunction>
void WithTargetThread(ProfileFunction profile_function) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));
  WithTargetThread(&scenario, profile_function);
}

Frames SampleScenario(UnwindScenario* scenario, ModuleCache* module_cache) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  Profile profile;
  WithTargetThread(scenario, [&](PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED);
    StackSamplingProfiler profiler(
        target_thread_id, params,
        std::make_unique<TestProfileBuilder>(
            module_cache,
            BindLambdaForTesting(
                [&profile, &sampling_thread_completed](Profile result_profile) {
                  profile = std::move(result_profile);
                  sampling_thread_completed.Signal();
                })));
    profiler.Start();
    sampling_thread_completed.Wait();
  });

  CHECK_EQ(1u, profile.frame_sets.size());
  return profile.frame_sets[0];
}

struct TestProfilerInfo {
  TestProfilerInfo(PlatformThreadId thread_id,
                   const SamplingParams& params,
                   ModuleCache* module_cache,
                   StackSamplerTestDelegate* delegate = nullptr)
      : completed(WaitableEvent::ResetPolicy::MANUAL,
                  WaitableEvent::InitialState::NOT_SIGNALED),
        profiler(thread_id,
                 params,
                 std::make_unique<TestProfileBuilder>(
                     module_cache,
                     BindLambdaForTesting([this](Profile result_profile) {
                       profile = std::move(result_profile);
                       completed.Signal();
                     })),
                 delegate) {}

  // The order here is important to ensure objects being referenced don't get
  // destructed until after the objects referencing them.
  Profile profile;
  WaitableEvent completed;
  StackSamplingProfiler profiler;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProfilerInfo);
};

// Creates multiple profilers based on a vector of parameters.
std::vector<std::unique_ptr<TestProfilerInfo>> CreateProfilers(
    PlatformThreadId target_thread_id,
    const std::vector<SamplingParams>& params,
    ModuleCache* module_cache) {
  DCHECK(!params.empty());

  std::vector<std::unique_ptr<TestProfilerInfo>> profilers;
  for (const auto& i : params) {
    profilers.push_back(
        std::make_unique<TestProfilerInfo>(target_thread_id, i, module_cache));
  }

  return profilers;
}

// Captures frames as specified by |params| on the TargetThread, and returns
// them. Waits up to |profiler_wait_time| for the profiler to complete.
FrameSets CaptureFrameSets(const SamplingParams& params,
                           TimeDelta profiler_wait_time,
                           ModuleCache* module_cache) {
  FrameSets frame_sets;
  WithTargetThread([&](PlatformThreadId target_thread_id) {
    TestProfilerInfo info(target_thread_id, params, module_cache);
    info.profiler.Start();
    info.completed.TimedWait(profiler_wait_time);
    info.profiler.Stop();
    info.completed.Wait();
    frame_sets = std::move(info.profile.frame_sets);
  });

  return frame_sets;
}

// Waits for one of multiple samplings to complete.
size_t WaitForSamplingComplete(
    const std::vector<std::unique_ptr<TestProfilerInfo>>& infos) {
  // Map unique_ptrs to something that WaitMany can accept.
  std::vector<WaitableEvent*> sampling_completed_rawptrs(infos.size());
  std::transform(infos.begin(), infos.end(), sampling_completed_rawptrs.begin(),
                 [](const std::unique_ptr<TestProfilerInfo>& info) {
                   return &info.get()->completed;
                 });
  // Wait for one profiler to finish.
  return WaitableEvent::WaitMany(sampling_completed_rawptrs.data(),
                                 sampling_completed_rawptrs.size());
}

// Returns a duration that is longer than the test timeout. We would use
// TimeDelta::Max() but https://crbug.com/465948.
TimeDelta AVeryLongTimeDelta() {
  return TimeDelta::FromDays(1);
}

// Tests the scenario where the library is unloaded after copying the stack, but
// before walking it. If |wait_until_unloaded| is true, ensures that the
// asynchronous library loading has completed before walking the stack. If
// false, the unloading may still be occurring during the stack walk.
void TestLibraryUnload(bool wait_until_unloaded, ModuleCache* module_cache) {
  // Test delegate that supports intervening between the copying of the stack
  // and the walking of the stack.
  class StackCopiedSignaler : public StackSamplerTestDelegate {
   public:
    StackCopiedSignaler(WaitableEvent* stack_copied,
                        WaitableEvent* start_stack_walk,
                        bool wait_to_walk_stack)
        : stack_copied_(stack_copied),
          start_stack_walk_(start_stack_walk),
          wait_to_walk_stack_(wait_to_walk_stack) {}

    void OnPreStackWalk() override {
      stack_copied_->Signal();
      if (wait_to_walk_stack_)
        start_stack_walk_->Wait();
    }

   private:
    WaitableEvent* const stack_copied_;
    WaitableEvent* const start_stack_walk_;
    const bool wait_to_walk_stack_;
  };

  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  NativeLibrary other_library = LoadOtherLibrary();

  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  UnwindScenario::SampleEvents events;
  TargetThread target_thread(
      BindLambdaForTesting([&]() { scenario.Execute(&events); }));

  PlatformThreadHandle target_thread_handle;
  EXPECT_TRUE(PlatformThread::Create(0, &target_thread, &target_thread_handle));

  events.ready_for_sample.Wait();

  WaitableEvent sampling_thread_completed(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  Profile profile;

  WaitableEvent stack_copied(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent start_stack_walk(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  StackCopiedSignaler test_delegate(&stack_copied, &start_stack_walk,
                                    wait_until_unloaded);
  StackSamplingProfiler profiler(
      target_thread.id(), params,
      std::make_unique<TestProfileBuilder>(
          module_cache,
          BindLambdaForTesting(
              [&profile, &sampling_thread_completed](Profile result_profile) {
                profile = std::move(result_profile);
                sampling_thread_completed.Signal();
              })),
      &test_delegate);

  profiler.Start();

  // Wait for the stack to be copied and the target thread to be resumed.
  stack_copied.Wait();

  // Cause the target thread to finish, so that it's no longer executing code in
  // the library we're about to unload.
  events.sample_finished.Signal();
  PlatformThread::Join(target_thread_handle);

  // Unload the library now that it's not being used.
  if (wait_until_unloaded)
    SynchronousUnloadNativeLibrary(other_library);
  else
    UnloadNativeLibrary(other_library);

  // Let the stack walk commence after unloading the library, if we're waiting
  // on that event.
  start_stack_walk.Signal();

  // Wait for the sampling thread to complete and fill out |profile|.
  sampling_thread_completed.Wait();

  // Look up the frames.
  ASSERT_EQ(1u, profile.frame_sets.size());
  const Frames& frames = profile.frame_sets[0];

  if (wait_until_unloaded) {
    // We expect the stack to look something like this, with the frame in the
    // now-unloaded library having a null module.
    //
    // ... WaitableEvent and system frames ...
    // WaitForSample()
    // TargetThread::OtherLibraryCallback
    // <frame in unloaded library>
    EXPECT_EQ(nullptr, frames.back().module)
        << "Stack:\n"
        << FormatSampleForDiagnosticOutput(frames);

    ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange()});
    ExpectStackDoesNotContain(frames,
                              {scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
  } else {
    // We didn't wait for the asynchronous unloading to complete, so the results
    // are non-deterministic: if the library finished unloading we should have
    // the same stack as |wait_until_unloaded|, if not we should have the full
    // stack. The important thing is that we should not crash.

    if (!frames.back().module) {
      // This is the same case as |wait_until_unloaded|.
      ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange()});
      ExpectStackDoesNotContain(frames,
                                {scenario.GetSetupFunctionAddressRange(),
                                 scenario.GetOuterFunctionAddressRange()});
      return;
    }

    ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange(),
                                 scenario.GetSetupFunctionAddressRange(),
                                 scenario.GetOuterFunctionAddressRange()});
  }
}

// Provide a suitable (and clean) environment for the tests below. All tests
// must use this class to ensure that proper clean-up is done and thus be
// usable in a later test.
class StackSamplingProfilerTest : public testing::Test {
 public:
  void SetUp() override {
    // The idle-shutdown time is too long for convenient (and accurate) testing.
    // That behavior is checked instead by artificially triggering it through
    // the TestPeer.
    StackSamplingProfiler::TestPeer::DisableIdleShutdown();
  }

  void TearDown() override {
    // Be a good citizen and clean up after ourselves. This also re-enables the
    // idle-shutdown behavior.
    StackSamplingProfiler::TestPeer::Reset();
  }

 protected:
  ModuleCache* module_cache() { return &module_cache_; }

 private:
  ModuleCache module_cache_;
};

}  // namespace

// Checks that the basic expected information is present in sampled frames.
//
// macOS ASAN is not yet supported - crbug.com/718628.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_MACOSX))
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_Basic) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));
  const Frames& frames = SampleScenario(&scenario, module_cache());

  // Check that all the modules are valid.
  for (const auto& frame : frames)
    EXPECT_NE(nullptr, frame.module);

  // The stack should contain a full unwind.
  ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// A simple unwinder that always generates one frame then aborts the stack walk.
class TestAuxUnwinder : public Unwinder {
 public:
  TestAuxUnwinder(const Frame& frame_to_report)
      : frame_to_report_(frame_to_report) {}

  TestAuxUnwinder(const TestAuxUnwinder&) = delete;
  TestAuxUnwinder& operator=(const TestAuxUnwinder&) = delete;

  bool CanUnwindFrom(const Frame* current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         ModuleCache* module_cache,
                         std::vector<Frame>* stack) const override {
    stack->push_back(frame_to_report_);
    return UnwindResult::ABORTED;
  }

 private:
  const Frame frame_to_report_;
};

// Checks that the profiler handles stacks containing dynamically-allocated
// stack memory.
// macOS ASAN is not yet supported - crbug.com/718628.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_MACOSX))
#define MAYBE_Alloca Alloca
#else
#define MAYBE_Alloca DISABLED_Alloca
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_Alloca) {
  UnwindScenario scenario(BindRepeating(&CallWithAlloca));
  const Frames& frames = SampleScenario(&scenario, module_cache());

  // The stack should contain a full unwind.
  ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that a stack that runs through another library produces a stack with
// the expected functions.
// macOS ASAN is not yet supported - crbug.com/718628.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_MACOSX))
#define MAYBE_OtherLibrary OtherLibrary
#else
#define MAYBE_OtherLibrary DISABLED_OtherLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_OtherLibrary) {
  ScopedNativeLibrary other_library(LoadOtherLibrary());
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library.get())));
  const Frames& frames = SampleScenario(&scenario, module_cache());

  // The stack should contain a full unwind.
  ExpectStackContains(frames, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that a stack that runs through a library that is unloading produces a
// stack, and doesn't crash.
// Unloading is synchronous on the Mac, so this test is inapplicable.
#if !defined(OS_MACOSX)
#define MAYBE_UnloadingLibrary UnloadingLibrary
#else
#define MAYBE_UnloadingLibrary DISABLED_UnloadingLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_UnloadingLibrary) {
  TestLibraryUnload(false, module_cache());
}

// Checks that a stack that runs through a library that has been unloaded
// produces a stack, and doesn't crash.
// macOS ASAN is not yet supported - crbug.com/718628.
#if !(defined(ADDRESS_SANITIZER) && defined(OS_MACOSX))
#define MAYBE_UnloadedLibrary UnloadedLibrary
#else
#define MAYBE_UnloadedLibrary DISABLED_UnloadedLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_UnloadedLibrary) {
  TestLibraryUnload(true, module_cache());
}

// Checks that a profiler can stop/destruct without ever having started.
PROFILER_TEST_F(StackSamplingProfilerTest, StopWithoutStarting) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params;
    params.sampling_interval = TimeDelta::FromMilliseconds(0);
    params.samples_per_profile = 1;

    Profile profile;
    WaitableEvent sampling_completed(WaitableEvent::ResetPolicy::MANUAL,
                                     WaitableEvent::InitialState::NOT_SIGNALED);

    StackSamplingProfiler profiler(
        target_thread_id, params,
        std::make_unique<TestProfileBuilder>(
            module_cache(),
            BindLambdaForTesting(
                [&profile, &sampling_completed](Profile result_profile) {
                  profile = std::move(result_profile);
                  sampling_completed.Signal();
                })));

    profiler.Stop();  // Constructed but never started.
    EXPECT_FALSE(sampling_completed.IsSignaled());
  });
}

// Checks that its okay to stop a profiler before it finishes even when the
// sampling thread continues to run.
PROFILER_TEST_F(StackSamplingProfilerTest, StopSafely) {
  // Test delegate that counts samples.
  class SampleRecordedCounter : public StackSamplerTestDelegate {
   public:
    SampleRecordedCounter() = default;

    void OnPreStackWalk() override {
      AutoLock lock(lock_);
      ++count_;
    }

    size_t Get() {
      AutoLock lock(lock_);
      return count_;
    }

   private:
    Lock lock_;
    size_t count_ = 0;
  };

  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params[2];

    // Providing an initial delay makes it more likely that both will be
    // scheduled before either starts to run. Once started, samples will
    // run ordered by their scheduled, interleaved times regardless of
    // whatever interval the thread wakes up.
    params[0].initial_delay = TimeDelta::FromMilliseconds(10);
    params[0].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[0].samples_per_profile = 100000;

    params[1].initial_delay = TimeDelta::FromMilliseconds(10);
    params[1].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[1].samples_per_profile = 100000;

    SampleRecordedCounter samples_recorded[size(params)];

    TestProfilerInfo profiler_info0(target_thread_id, params[0], module_cache(),
                                    &samples_recorded[0]);
    TestProfilerInfo profiler_info1(target_thread_id, params[1], module_cache(),
                                    &samples_recorded[1]);

    profiler_info0.profiler.Start();
    profiler_info1.profiler.Start();

    // Wait for both to start accumulating samples. Using a WaitableEvent is
    // possible but gets complicated later on because there's no way of knowing
    // if 0 or 1 additional sample will be taken after Stop() and thus no way
    // of knowing how many Wait() calls to make on it.
    while (samples_recorded[0].Get() == 0 || samples_recorded[1].Get() == 0)
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));

    // Ensure that the first sampler can be safely stopped while the second
    // continues to run. The stopped first profiler will still have a
    // RecordSampleTask pending that will do nothing when executed because the
    // collection will have been removed by Stop().
    profiler_info0.profiler.Stop();
    profiler_info0.completed.Wait();
    size_t count0 = samples_recorded[0].Get();
    size_t count1 = samples_recorded[1].Get();

    // Waiting for the second sampler to collect a couple samples ensures that
    // the pending RecordSampleTask for the first has executed because tasks are
    // always ordered by their next scheduled time.
    while (samples_recorded[1].Get() < count1 + 2)
      PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));

    // Ensure that the first profiler didn't do anything since it was stopped.
    EXPECT_EQ(count0, samples_recorded[0].Get());
  });
}

// Checks that no frames are captured if the profiling is stopped during the
// initial delay.
PROFILER_TEST_F(StackSamplingProfilerTest, StopDuringInitialDelay) {
  SamplingParams params;
  params.initial_delay = TimeDelta::FromSeconds(60);

  FrameSets frame_sets =
      CaptureFrameSets(params, TimeDelta::FromMilliseconds(0), module_cache());

  EXPECT_TRUE(frame_sets.empty());
}

// Checks that tasks can be stopped before completion and incomplete frames are
// captured.
PROFILER_TEST_F(StackSamplingProfilerTest, StopDuringInterSampleInterval) {
  // Test delegate that counts samples.
  class SampleRecordedEvent : public StackSamplerTestDelegate {
   public:
    SampleRecordedEvent()
        : sample_recorded_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED) {}

    void OnPreStackWalk() override { sample_recorded_.Signal(); }

    void WaitForSample() { sample_recorded_.Wait(); }

   private:
    WaitableEvent sample_recorded_;
  };

  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params;

    params.sampling_interval = AVeryLongTimeDelta();
    params.samples_per_profile = 2;

    SampleRecordedEvent samples_recorded;
    TestProfilerInfo profiler_info(target_thread_id, params, module_cache(),
                                   &samples_recorded);

    profiler_info.profiler.Start();

    // Wait for profiler to start accumulating samples.
    samples_recorded.WaitForSample();

    // Ensure that it can stop safely.
    profiler_info.profiler.Stop();
    profiler_info.completed.Wait();

    EXPECT_EQ(1u, profiler_info.profile.frame_sets.size());
  });
}

// Checks that we can destroy the profiler while profiling.
PROFILER_TEST_F(StackSamplingProfilerTest, DestroyProfilerWhileProfiling) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(10);

  Profile profile;
  WithTargetThread([&, this](PlatformThreadId target_thread_id) {
    std::unique_ptr<StackSamplingProfiler> profiler;
    auto profile_builder = std::make_unique<TestProfileBuilder>(
        module_cache(),
        BindLambdaForTesting([&profile](Profile result_profile) {
          profile = std::move(result_profile);
        }));
    profiler.reset(new StackSamplingProfiler(target_thread_id, params,
                                             std::move(profile_builder)));
    profiler->Start();
    profiler.reset();

    // Wait longer than a sample interval to catch any use-after-free actions by
    // the profiler thread.
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(50));
  });
}

// Checks that the different profilers may be run.
PROFILER_TEST_F(StackSamplingProfilerTest, CanRunMultipleProfilers) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  FrameSets frame_sets =
      CaptureFrameSets(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, frame_sets.size());

  frame_sets = CaptureFrameSets(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, frame_sets.size());
}

// Checks that a sampler can be started while another is running.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleStart) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    std::vector<SamplingParams> params(2);

    params[0].initial_delay = AVeryLongTimeDelta();
    params[0].samples_per_profile = 1;

    params[1].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[1].samples_per_profile = 1;

    std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos =
        CreateProfilers(target_thread_id, params, module_cache());

    profiler_infos[0]->profiler.Start();
    profiler_infos[1]->profiler.Start();
    profiler_infos[1]->completed.Wait();
    EXPECT_EQ(1u, profiler_infos[1]->profile.frame_sets.size());
  });
}

// Checks that the profile duration and the sampling interval are calculated
// correctly. Also checks that RecordMetadata() is invoked each time a sample
// is recorded.
PROFILER_TEST_F(StackSamplingProfilerTest, ProfileGeneralInfo) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params;
    params.sampling_interval = TimeDelta::FromMilliseconds(1);
    params.samples_per_profile = 3;

    TestProfilerInfo profiler_info(target_thread_id, params, module_cache());

    profiler_info.profiler.Start();
    profiler_info.completed.Wait();
    EXPECT_EQ(3u, profiler_info.profile.frame_sets.size());

    // The profile duration should be greater than the total sampling intervals.
    EXPECT_GT(profiler_info.profile.profile_duration,
              profiler_info.profile.sampling_period * 3);

    EXPECT_EQ(TimeDelta::FromMilliseconds(1),
              profiler_info.profile.sampling_period);

    // The number of invocations of RecordMetadata() should be equal to the
    // number of samples recorded.
    EXPECT_EQ(3, profiler_info.profile.metadata_count);
  });
}

// Checks that the sampling thread can shut down.
PROFILER_TEST_F(StackSamplingProfilerTest, SamplerIdleShutdown) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  FrameSets frame_sets =
      CaptureFrameSets(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, frame_sets.size());

  // Capture thread should still be running at this point.
  ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

  // Initiate an "idle" shutdown and ensure it happens. Idle-shutdown was
  // disabled by the test fixture so the test will fail due to a timeout if
  // it does not exit.
  StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(false);

  // While the shutdown has been initiated, the actual exit of the thread still
  // happens asynchronously. Watch until the thread actually exits. This test
  // will time-out in the case of failure.
  while (StackSamplingProfiler::TestPeer::IsSamplingThreadRunning())
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(1));
}

// Checks that additional requests will restart a stopped profiler.
PROFILER_TEST_F(StackSamplingProfilerTest,
                WillRestartSamplerAfterIdleShutdown) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  FrameSets frame_sets =
      CaptureFrameSets(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, frame_sets.size());

  // Capture thread should still be running at this point.
  ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

  // Post a ShutdownTask on the sampling thread which, when executed, will
  // mark the thread as EXITING and begin shut down of the thread.
  StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(false);

  // Ensure another capture will start the sampling thread and run.
  frame_sets = CaptureFrameSets(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, frame_sets.size());
  EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
}

// Checks that it's safe to stop a task after it's completed and the sampling
// thread has shut-down for being idle.
PROFILER_TEST_F(StackSamplingProfilerTest, StopAfterIdleShutdown) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params;

    params.sampling_interval = TimeDelta::FromMilliseconds(1);
    params.samples_per_profile = 1;

    TestProfilerInfo profiler_info(target_thread_id, params, module_cache());

    profiler_info.profiler.Start();
    profiler_info.completed.Wait();

    // Capture thread should still be running at this point.
    ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

    // Perform an idle shutdown.
    StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(false);

    // Stop should be safe though its impossible to know at this moment if the
    // sampling thread has completely exited or will just "stop soon".
    profiler_info.profiler.Stop();
  });
}

// Checks that profilers can run both before and after the sampling thread has
// started.
PROFILER_TEST_F(StackSamplingProfilerTest,
                ProfileBeforeAndAfterSamplingThreadRunning) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    std::vector<SamplingParams> params(2);

    params[0].initial_delay = AVeryLongTimeDelta();
    params[0].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[0].samples_per_profile = 1;

    params[1].initial_delay = TimeDelta::FromMilliseconds(0);
    params[1].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[1].samples_per_profile = 1;

    std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos =
        CreateProfilers(target_thread_id, params, module_cache());

    // First profiler is started when there has never been a sampling thread.
    EXPECT_FALSE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
    profiler_infos[0]->profiler.Start();
    // Second profiler is started when sampling thread is already running.
    EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
    profiler_infos[1]->profiler.Start();

    // Only the second profiler should finish before test times out.
    size_t completed_profiler = WaitForSamplingComplete(profiler_infos);
    EXPECT_EQ(1U, completed_profiler);
  });
}

// Checks that an idle-shutdown task will abort if a new profiler starts
// between when it was posted and when it runs.
PROFILER_TEST_F(StackSamplingProfilerTest, IdleShutdownAbort) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    SamplingParams params;

    params.sampling_interval = TimeDelta::FromMilliseconds(1);
    params.samples_per_profile = 1;

    TestProfilerInfo profiler_info(target_thread_id, params, module_cache());

    profiler_info.profiler.Start();
    profiler_info.completed.Wait();
    EXPECT_EQ(1u, profiler_info.profile.frame_sets.size());

    // Perform an idle shutdown but simulate that a new capture is started
    // before it can actually run.
    StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(true);

    // Though the shutdown-task has been executed, any actual exit of the
    // thread is asynchronous so there is no way to detect that *didn't* exit
    // except to wait a reasonable amount of time and then check. Since the
    // thread was just running ("perform" blocked until it was), it should
    // finish almost immediately and without any waiting for tasks or events.
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(200));
    EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

    // Ensure that it's still possible to run another sampler.
    TestProfilerInfo another_info(target_thread_id, params, module_cache());
    another_info.profiler.Start();
    another_info.completed.Wait();
    EXPECT_EQ(1u, another_info.profile.frame_sets.size());
  });
}

// Checks that synchronized multiple sampling requests execute in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, ConcurrentProfiling_InSync) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    std::vector<SamplingParams> params(2);

    // Providing an initial delay makes it more likely that both will be
    // scheduled before either starts to run. Once started, samples will
    // run ordered by their scheduled, interleaved times regardless of
    // whatever interval the thread wakes up. Thus, total execution time
    // will be 10ms (delay) + 10x1ms (sampling) + 1/2 timer minimum interval.
    params[0].initial_delay = TimeDelta::FromMilliseconds(10);
    params[0].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[0].samples_per_profile = 9;

    params[1].initial_delay = TimeDelta::FromMilliseconds(11);
    params[1].sampling_interval = TimeDelta::FromMilliseconds(1);
    params[1].samples_per_profile = 8;

    std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos =
        CreateProfilers(target_thread_id, params, module_cache());

    profiler_infos[0]->profiler.Start();
    profiler_infos[1]->profiler.Start();

    // Wait for one profiler to finish.
    size_t completed_profiler = WaitForSamplingComplete(profiler_infos);

    size_t other_profiler = 1 - completed_profiler;
    // Wait for the other profiler to finish.
    profiler_infos[other_profiler]->completed.Wait();

    // Ensure each got the correct number of frame sets.
    EXPECT_EQ(9u, profiler_infos[0]->profile.frame_sets.size());
    EXPECT_EQ(8u, profiler_infos[1]->profile.frame_sets.size());
  });
}

// Checks that several mixed sampling requests execute in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, ConcurrentProfiling_Mixed) {
  WithTargetThread([this](PlatformThreadId target_thread_id) {
    std::vector<SamplingParams> params(3);

    params[0].initial_delay = TimeDelta::FromMilliseconds(8);
    params[0].sampling_interval = TimeDelta::FromMilliseconds(4);
    params[0].samples_per_profile = 10;

    params[1].initial_delay = TimeDelta::FromMilliseconds(9);
    params[1].sampling_interval = TimeDelta::FromMilliseconds(3);
    params[1].samples_per_profile = 10;

    params[2].initial_delay = TimeDelta::FromMilliseconds(10);
    params[2].sampling_interval = TimeDelta::FromMilliseconds(2);
    params[2].samples_per_profile = 10;

    std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos =
        CreateProfilers(target_thread_id, params, module_cache());

    for (auto& i : profiler_infos)
      i->profiler.Start();

    // Wait for one profiler to finish.
    size_t completed_profiler = WaitForSamplingComplete(profiler_infos);
    EXPECT_EQ(10u,
              profiler_infos[completed_profiler]->profile.frame_sets.size());
    // Stop and destroy all profilers, always in the same order. Don't crash.
    for (auto& i : profiler_infos)
      i->profiler.Stop();
    for (auto& i : profiler_infos)
      i.reset();
  });
}

// Checks that different threads can be sampled in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleSampledThreads) {
  UnwindScenario scenario1(BindRepeating(&CallWithPlainFunction));
  UnwindScenario::SampleEvents events1;
  TargetThread target_thread1(
      BindLambdaForTesting([&]() { scenario1.Execute(&events1); }));
  PlatformThreadHandle target_thread_handle1;
  EXPECT_TRUE(
      PlatformThread::Create(0, &target_thread1, &target_thread_handle1));
  events1.ready_for_sample.Wait();

  UnwindScenario scenario2(BindRepeating(&CallWithPlainFunction));
  UnwindScenario::SampleEvents events2;
  TargetThread target_thread2(
      BindLambdaForTesting([&]() { scenario2.Execute(&events2); }));

  PlatformThreadHandle target_thread_handle2;
  EXPECT_TRUE(
      PlatformThread::Create(0, &target_thread2, &target_thread_handle2));
  events2.ready_for_sample.Wait();

  // Providing an initial delay makes it more likely that both will be
  // scheduled before either starts to run. Once started, samples will
  // run ordered by their scheduled, interleaved times regardless of
  // whatever interval the thread wakes up.
  SamplingParams params1, params2;
  params1.initial_delay = TimeDelta::FromMilliseconds(10);
  params1.sampling_interval = TimeDelta::FromMilliseconds(1);
  params1.samples_per_profile = 9;
  params2.initial_delay = TimeDelta::FromMilliseconds(10);
  params2.sampling_interval = TimeDelta::FromMilliseconds(1);
  params2.samples_per_profile = 8;

  Profile profile1, profile2;

  WaitableEvent sampling_thread_completed1(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  StackSamplingProfiler profiler1(
      target_thread1.id(), params1,
      std::make_unique<TestProfileBuilder>(
          module_cache(),
          BindLambdaForTesting(
              [&profile1, &sampling_thread_completed1](Profile result_profile) {
                profile1 = std::move(result_profile);
                sampling_thread_completed1.Signal();
              })));

  WaitableEvent sampling_thread_completed2(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  StackSamplingProfiler profiler2(
      target_thread2.id(), params2,
      std::make_unique<TestProfileBuilder>(
          module_cache(),
          BindLambdaForTesting(
              [&profile2, &sampling_thread_completed2](Profile result_profile) {
                profile2 = std::move(result_profile);
                sampling_thread_completed2.Signal();
              })));

  // Finally the real work.
  profiler1.Start();
  profiler2.Start();
  sampling_thread_completed1.Wait();
  sampling_thread_completed2.Wait();
  EXPECT_EQ(9u, profile1.frame_sets.size());
  EXPECT_EQ(8u, profile2.frame_sets.size());

  events1.sample_finished.Signal();
  events2.sample_finished.Signal();
  PlatformThread::Join(target_thread_handle1);
  PlatformThread::Join(target_thread_handle2);
}

// A simple thread that runs a profiler on another thread.
class ProfilerThread : public SimpleThread {
 public:
  ProfilerThread(const std::string& name,
                 PlatformThreadId thread_id,
                 const SamplingParams& params,
                 ModuleCache* module_cache)
      : SimpleThread(name, Options()),
        run_(WaitableEvent::ResetPolicy::MANUAL,
             WaitableEvent::InitialState::NOT_SIGNALED),
        completed_(WaitableEvent::ResetPolicy::MANUAL,
                   WaitableEvent::InitialState::NOT_SIGNALED),
        profiler_(thread_id,
                  params,
                  std::make_unique<TestProfileBuilder>(
                      module_cache,
                      BindLambdaForTesting([this](Profile result_profile) {
                        profile_ = std::move(result_profile);
                        completed_.Signal();
                      }))) {}

  void Run() override {
    run_.Wait();
    profiler_.Start();
  }

  void Go() { run_.Signal(); }

  void Wait() { completed_.Wait(); }

  Profile& profile() { return profile_; }

 private:
  WaitableEvent run_;

  Profile profile_;
  WaitableEvent completed_;
  StackSamplingProfiler profiler_;
};

// Checks that different threads can run samplers in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleProfilerThreads) {
  WithTargetThread([](PlatformThreadId target_thread_id) {
    // Providing an initial delay makes it more likely that both will be
    // scheduled before either starts to run. Once started, samples will
    // run ordered by their scheduled, interleaved times regardless of
    // whatever interval the thread wakes up.
    SamplingParams params1, params2;
    params1.initial_delay = TimeDelta::FromMilliseconds(10);
    params1.sampling_interval = TimeDelta::FromMilliseconds(1);
    params1.samples_per_profile = 9;
    params2.initial_delay = TimeDelta::FromMilliseconds(10);
    params2.sampling_interval = TimeDelta::FromMilliseconds(1);
    params2.samples_per_profile = 8;

    // Start the profiler threads and give them a moment to get going.
    ModuleCache module_cache1;
    ProfilerThread profiler_thread1("profiler1", target_thread_id, params1,
                                    &module_cache1);
    ModuleCache module_cache2;
    ProfilerThread profiler_thread2("profiler2", target_thread_id, params2,
                                    &module_cache2);
    profiler_thread1.Start();
    profiler_thread2.Start();
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));

    // This will (approximately) synchronize the two threads.
    profiler_thread1.Go();
    profiler_thread2.Go();

    // Wait for them both to finish and validate collection.
    profiler_thread1.Wait();
    profiler_thread2.Wait();
    EXPECT_EQ(9u, profiler_thread1.profile().frame_sets.size());
    EXPECT_EQ(8u, profiler_thread2.profile().frame_sets.size());

    profiler_thread1.Join();
    profiler_thread2.Join();
  });
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_BeforeStart) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  Profile profile;
  WithTargetThread(&scenario, [&](PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED);
    StackSamplingProfiler profiler(
        target_thread_id, params,
        std::make_unique<TestProfileBuilder>(
            module_cache(),
            BindLambdaForTesting(
                [&profile, &sampling_thread_completed](Profile result_profile) {
                  profile = std::move(result_profile);
                  sampling_thread_completed.Signal();
                })));
    profiler.AddAuxUnwinder(
        std::make_unique<TestAuxUnwinder>(Frame(23, nullptr)));
    profiler.Start();
    sampling_thread_completed.Wait();
  });

  // The sample should have one frame from the context values and one from the
  // TestAuxUnwinder.
  ASSERT_EQ(1u, profile.frame_sets.size());
  const Frames& frames = profile.frame_sets[0];

  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(23u, frames[1].instruction_pointer);
  EXPECT_EQ(nullptr, frames[1].module);
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_AfterStart) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  Profile profile;
  WithTargetThread(&scenario, [&](PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED);
    StackSamplingProfiler profiler(
        target_thread_id, params,
        std::make_unique<TestProfileBuilder>(
            module_cache(),
            BindLambdaForTesting(
                [&profile, &sampling_thread_completed](Profile result_profile) {
                  profile = std::move(result_profile);
                  sampling_thread_completed.Signal();
                })));
    profiler.Start();
    profiler.AddAuxUnwinder(
        std::make_unique<TestAuxUnwinder>(Frame(23, nullptr)));
    sampling_thread_completed.Wait();
  });

  // The sample should have one frame from the context values and one from the
  // TestAuxUnwinder.
  ASSERT_EQ(1u, profile.frame_sets.size());
  const Frames& frames = profile.frame_sets[0];

  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(23u, frames[1].instruction_pointer);
  EXPECT_EQ(nullptr, frames[1].module);
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_AfterStop) {
  SamplingParams params;
  params.sampling_interval = TimeDelta::FromMilliseconds(0);
  params.samples_per_profile = 1;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  Profile profile;
  WithTargetThread(&scenario, [&](PlatformThreadId target_thread_id) {
    WaitableEvent sampling_thread_completed(
        WaitableEvent::ResetPolicy::MANUAL,
        WaitableEvent::InitialState::NOT_SIGNALED);
    StackSamplingProfiler profiler(
        target_thread_id, params,
        std::make_unique<TestProfileBuilder>(
            module_cache(),
            BindLambdaForTesting(
                [&profile, &sampling_thread_completed](Profile result_profile) {
                  profile = std::move(result_profile);
                  sampling_thread_completed.Signal();
                })));
    profiler.Start();
    profiler.Stop();
    profiler.AddAuxUnwinder(
        std::make_unique<TestAuxUnwinder>(Frame(23, nullptr)));
    sampling_thread_completed.Wait();
  });

  // The AuxUnwinder should be accepted without error. It will have no effect
  // since the collection has stopped.
}

}  // namespace base
