// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <algorithm>
#include <cmath>

#include "base/allocator/allocator_shim.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/atomicops.h"
#include "base/debug/stack_trace.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/partition_alloc_buildflags.h"
#include "base/rand_util.h"
#include "base/threading/thread_local_storage.h"
#include "build/build_config.h"

namespace base {

using base::allocator::AllocatorDispatch;
using base::subtle::Atomic32;
using base::subtle::AtomicWord;

namespace {

// Control how many top frames to skip when recording call stack.
// These frames correspond to the profiler own frames.
const uint32_t kSkipBaseAllocatorFrames = 2;

const size_t kDefaultSamplingIntervalBytes = 128 * 1024;

// Controls if sample intervals should not be randomized. Used for testing.
bool g_deterministic;

// A positive value if profiling is running, otherwise it's zero.
Atomic32 g_running;

// Number of lock-free safe (not causing rehashing) accesses to samples_ map
// currently being performed.
Atomic32 g_operations_in_flight;

// Controls if new incoming lock-free accesses are allowed.
// When set to true, threads should not enter lock-free paths.
Atomic32 g_fast_path_is_closed;

// Sampling interval parameter, the mean value for intervals between samples.
AtomicWord g_sampling_interval = kDefaultSamplingIntervalBytes;

// Last generated sample ordinal number.
uint32_t g_last_sample_ordinal = 0;

void (*g_hooks_install_callback)();
Atomic32 g_hooks_installed;

void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  void* address = self->next->alloc_function(self->next, size, context);
  SamplingHeapProfiler::RecordAlloc(address, size, kSkipBaseAllocatorFrames);
  return address;
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  void* address =
      self->next->alloc_zero_initialized_function(self->next, n, size, context);
  SamplingHeapProfiler::RecordAlloc(address, n * size,
                                    kSkipBaseAllocatorFrames);
  return address;
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  void* address =
      self->next->alloc_aligned_function(self->next, alignment, size, context);
  SamplingHeapProfiler::RecordAlloc(address, size, kSkipBaseAllocatorFrames);
  return address;
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  // Note: size == 0 actually performs free.
  SamplingHeapProfiler::RecordFree(address);
  address = self->next->realloc_function(self->next, address, size, context);
  SamplingHeapProfiler::RecordAlloc(address, size, kSkipBaseAllocatorFrames);
  return address;
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {
  SamplingHeapProfiler::RecordFree(address);
  self->next->free_function(self->next, address, context);
}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  unsigned num_allocated = self->next->batch_malloc_function(
      self->next, size, results, num_requested, context);
  for (unsigned i = 0; i < num_allocated; ++i) {
    SamplingHeapProfiler::RecordAlloc(results[i], size,
                                      kSkipBaseAllocatorFrames);
  }
  return num_allocated;
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i)
    SamplingHeapProfiler::RecordFree(to_be_freed[i]);
  self->next->batch_free_function(self->next, to_be_freed, num_to_be_freed,
                                  context);
}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {
  SamplingHeapProfiler::RecordFree(address);
  self->next->free_definite_size_function(self->next, address, size, context);
}

AllocatorDispatch g_allocator_dispatch = {&AllocFn,
                                          &AllocZeroInitializedFn,
                                          &AllocAlignedFn,
                                          &ReallocFn,
                                          &FreeFn,
                                          &GetSizeEstimateFn,
                                          &BatchMallocFn,
                                          &BatchFreeFn,
                                          &FreeDefiniteSizeFn,
                                          nullptr};

#if BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

void PartitionAllocHook(void* address, size_t size, const char*) {
  SamplingHeapProfiler::RecordAlloc(address, size);
}

void PartitionFreeHook(void* address) {
  SamplingHeapProfiler::RecordFree(address);
}

#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

ThreadLocalStorage::Slot& AccumulatedBytesTLS() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot>
      accumulated_bytes_tls;
  return *accumulated_bytes_tls;
}

}  // namespace

SamplingHeapProfiler::Sample::Sample(size_t size,
                                     size_t total,
                                     uint32_t ordinal)
    : size(size), total(total), ordinal(ordinal) {}

SamplingHeapProfiler::Sample::Sample(const Sample&) = default;

SamplingHeapProfiler::Sample::~Sample() = default;

SamplingHeapProfiler* SamplingHeapProfiler::instance_;

SamplingHeapProfiler::SamplingHeapProfiler() {
  instance_ = this;
}

// static
void SamplingHeapProfiler::InitTLSSlot() {
  // Preallocate the TLS slot early, so it can't cause reentracy issues
  // when sampling is started.
  ignore_result(AccumulatedBytesTLS().Get());
}

// static
void SamplingHeapProfiler::InstallAllocatorHooksOnce() {
  static bool hook_installed = InstallAllocatorHooks();
  ignore_result(hook_installed);
}

// static
bool SamplingHeapProfiler::InstallAllocatorHooks() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  base::allocator::InsertAllocatorDispatch(&g_allocator_dispatch);
#else
  ignore_result(g_allocator_dispatch);
  DLOG(WARNING)
      << "base::allocator shims are not available for memory sampling.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

#if BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)
  base::PartitionAllocHooks::SetAllocationHook(&PartitionAllocHook);
  base::PartitionAllocHooks::SetFreeHook(&PartitionFreeHook);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC) && !defined(OS_NACL)

  int32_t hooks_install_callback_has_been_set =
      base::subtle::Acquire_CompareAndSwap(&g_hooks_installed, 0, 1);
  if (hooks_install_callback_has_been_set)
    g_hooks_install_callback();

  return true;
}

// static
void SamplingHeapProfiler::SetHooksInstallCallback(
    void (*hooks_install_callback)()) {
  CHECK(!g_hooks_install_callback && hooks_install_callback);
  g_hooks_install_callback = hooks_install_callback;

  int32_t profiler_has_already_been_initialized =
      base::subtle::Release_CompareAndSwap(&g_hooks_installed, 0, 1);
  if (profiler_has_already_been_initialized)
    g_hooks_install_callback();
}

uint32_t SamplingHeapProfiler::Start() {
  InstallAllocatorHooksOnce();
  base::subtle::Barrier_AtomicIncrement(&g_running, 1);
  return g_last_sample_ordinal;
}

void SamplingHeapProfiler::Stop() {
  AtomicWord count = base::subtle::Barrier_AtomicIncrement(&g_running, -1);
  CHECK_GE(count, 0);
}

void SamplingHeapProfiler::SetSamplingInterval(size_t sampling_interval) {
  // TODO(alph): Reset the sample being collected if running.
  base::subtle::Release_Store(&g_sampling_interval,
                              static_cast<AtomicWord>(sampling_interval));
}

// static
size_t SamplingHeapProfiler::GetNextSampleInterval(size_t interval) {
  if (UNLIKELY(g_deterministic))
    return interval;

  // We sample with a Poisson process, with constant average sampling
  // interval. This follows the exponential probability distribution with
  // parameter λ = 1/interval where |interval| is the average number of bytes
  // between samples.
  // Let u be a uniformly distributed random number between 0 and 1, then
  // next_sample = -ln(u) / λ
  double uniform = base::RandDouble();
  double value = -log(uniform) * interval;
  size_t min_value = sizeof(intptr_t);
  // We limit the upper bound of a sample interval to make sure we don't have
  // huge gaps in the sampling stream. Probability of the upper bound gets hit
  // is exp(-20) ~ 2e-9, so it should not skew the distibution.
  size_t max_value = interval * 20;
  if (UNLIKELY(value < min_value))
    return min_value;
  if (UNLIKELY(value > max_value))
    return max_value;
  return static_cast<size_t>(value);
}

// static
void SamplingHeapProfiler::RecordAlloc(void* address,
                                       size_t size,
                                       uint32_t skip_frames) {
  if (UNLIKELY(!base::subtle::NoBarrier_Load(&g_running)))
    return;

  // TODO(alph): On MacOS it may call the hook several times for a single
  // allocation. Handle the case.

  intptr_t accumulated_bytes =
      reinterpret_cast<intptr_t>(AccumulatedBytesTLS().Get());
  accumulated_bytes += size;
  if (LIKELY(accumulated_bytes < 0)) {
    AccumulatedBytesTLS().Set(reinterpret_cast<void*>(accumulated_bytes));
    return;
  }

  size_t mean_interval = base::subtle::NoBarrier_Load(&g_sampling_interval);
  size_t samples = accumulated_bytes / mean_interval;
  accumulated_bytes %= mean_interval;

  do {
    accumulated_bytes -= GetNextSampleInterval(mean_interval);
    ++samples;
  } while (accumulated_bytes >= 0);

  AccumulatedBytesTLS().Set(reinterpret_cast<void*>(accumulated_bytes));

  instance_->DoRecordAlloc(samples * mean_interval, size, address, skip_frames);
}

void SamplingHeapProfiler::RecordStackTrace(Sample* sample,
                                            uint32_t skip_frames) {
#if !defined(OS_NACL)
  // TODO(alph): Consider using debug::TraceStackFramePointers. It should be
  // somewhat faster than base::debug::StackTrace.
  base::debug::StackTrace trace;
  size_t count;
  void* const* addresses = const_cast<void* const*>(trace.Addresses(&count));
  const uint32_t kSkipProfilerOwnFrames = 2;
  skip_frames += kSkipProfilerOwnFrames;
  sample->stack.insert(
      sample->stack.end(), &addresses[skip_frames],
      &addresses[std::max(count, static_cast<size_t>(skip_frames))]);
#endif
}

void SamplingHeapProfiler::DoRecordAlloc(size_t total_allocated,
                                         size_t size,
                                         void* address,
                                         uint32_t skip_frames) {
  if (entered_.Get())
    return;
  base::AutoLock lock(mutex_);
  entered_.Set(true);

  Sample sample(size, total_allocated, ++g_last_sample_ordinal);
  RecordStackTrace(&sample, skip_frames);

  // Close the fast-path as inserting an element into samples_ may cause
  // rehashing that invalidates iterators affecting all the concurrent
  // readers.
  base::subtle::Release_Store(&g_fast_path_is_closed, 1);
  while (base::subtle::Acquire_Load(&g_operations_in_flight)) {
    while (base::subtle::NoBarrier_Load(&g_operations_in_flight)) {
    }
  }
  for (auto* observer : observers_)
    observer->SampleAdded(sample.ordinal, size, total_allocated);
  // TODO(alph): We can do better by keeping the fast-path open when
  // we know insert won't cause rehashing.
  samples_.emplace(address, std::move(sample));
  base::subtle::Release_Store(&g_fast_path_is_closed, 0);

  entered_.Set(false);
}

// static
void SamplingHeapProfiler::RecordFree(void* address) {
  bool maybe_sampled = true;  // Pessimistically assume allocation was sampled.
  base::subtle::Barrier_AtomicIncrement(&g_operations_in_flight, 1);
  if (LIKELY(!base::subtle::NoBarrier_Load(&g_fast_path_is_closed)))
    maybe_sampled = instance_->samples_.count(address);
  base::subtle::Barrier_AtomicIncrement(&g_operations_in_flight, -1);
  if (maybe_sampled)
    instance_->DoRecordFree(address);
}

void SamplingHeapProfiler::DoRecordFree(void* address) {
  if (entered_.Get())
    return;
  base::AutoLock lock(mutex_);
  entered_.Set(true);
  auto it = samples_.find(address);
  if (it != samples_.end()) {
    for (auto* observer : observers_)
      observer->SampleRemoved(it->second.ordinal);
    samples_.erase(it);
  }
  entered_.Set(false);
}

// static
SamplingHeapProfiler* SamplingHeapProfiler::GetInstance() {
  static base::NoDestructor<SamplingHeapProfiler> instance;
  return instance.get();
}

// static
void SamplingHeapProfiler::SuppressRandomnessForTest(bool suppress) {
  g_deterministic = suppress;
}

void SamplingHeapProfiler::AddSamplesObserver(SamplesObserver* observer) {
  base::AutoLock lock(mutex_);
  CHECK(!entered_.Get());
  entered_.Set(true);
  observers_.push_back(observer);
  entered_.Set(false);
}

void SamplingHeapProfiler::RemoveSamplesObserver(SamplesObserver* observer) {
  base::AutoLock lock(mutex_);
  CHECK(!entered_.Get());
  entered_.Set(true);
  auto it = std::find(observers_.begin(), observers_.end(), observer);
  CHECK(it != observers_.end());
  observers_.erase(it);
  entered_.Set(false);
}

std::vector<SamplingHeapProfiler::Sample> SamplingHeapProfiler::GetSamples(
    uint32_t profile_id) {
  base::AutoLock lock(mutex_);
  CHECK(!entered_.Get());
  entered_.Set(true);
  std::vector<Sample> samples;
  for (auto& it : samples_) {
    Sample& sample = it.second;
    if (sample.ordinal > profile_id)
      samples.push_back(sample);
  }
  entered_.Set(false);
  return samples;
}

}  // namespace base
