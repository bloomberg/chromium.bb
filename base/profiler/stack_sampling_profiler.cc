// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/profiler/native_stack_sampler.h"
#include "base/synchronization/lock.h"
#include "base/timer/elapsed_timer.h"

namespace base {

// PendingProfiles ------------------------------------------------------------

namespace {

// Thread-safe singleton class that stores collected call stack profiles waiting
// to be processed.
class PendingProfiles {
 public:
  ~PendingProfiles();

  static PendingProfiles* GetInstance();

  // Appends |profiles| to |profiles_|. This function may be called on any
  // thread.
  void AppendProfiles(
      const std::vector<StackSamplingProfiler::CallStackProfile>& profiles);

  // Copies the pending profiles from |profiles_| into |profiles|, and clears
  // |profiles_|. This function may be called on any thread.
  void GetAndClearPendingProfiles(
      std::vector<StackSamplingProfiler::CallStackProfile>* profiles);

 private:
  friend struct DefaultSingletonTraits<PendingProfiles>;

  PendingProfiles();

  Lock profiles_lock_;
  std::vector<StackSamplingProfiler::CallStackProfile> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PendingProfiles);
};

PendingProfiles::PendingProfiles() {}

PendingProfiles::~PendingProfiles() {}

// static
PendingProfiles* PendingProfiles::GetInstance() {
  return Singleton<PendingProfiles>::get();
}

void PendingProfiles::AppendProfiles(
    const std::vector<StackSamplingProfiler::CallStackProfile>& profiles) {
  AutoLock scoped_lock(profiles_lock_);
  profiles_.insert(profiles_.end(), profiles.begin(), profiles.end());
}

void PendingProfiles::GetAndClearPendingProfiles(
    std::vector<StackSamplingProfiler::CallStackProfile>* profiles) {
  profiles->clear();

  AutoLock scoped_lock(profiles_lock_);
  profiles_.swap(*profiles);
}

}  // namespace

// StackSamplingProfiler::Module ----------------------------------------------

StackSamplingProfiler::Module::Module() : base_address(nullptr) {}
StackSamplingProfiler::Module::Module(const void* base_address,
                                      const std::string& id,
                                      const FilePath& filename)
    : base_address(base_address), id(id), filename(filename) {}

StackSamplingProfiler::Module::~Module() {}

// StackSamplingProfiler::Frame -----------------------------------------------

StackSamplingProfiler::Frame::Frame(const void* instruction_pointer,
                                    size_t module_index)
    : instruction_pointer(instruction_pointer),
      module_index(module_index) {}

StackSamplingProfiler::Frame::~Frame() {}

// StackSamplingProfiler::CallStackProfile ------------------------------------

StackSamplingProfiler::CallStackProfile::CallStackProfile()
    : preserve_sample_ordering(false) {}

StackSamplingProfiler::CallStackProfile::~CallStackProfile() {}

// StackSamplingProfiler::SamplingThread --------------------------------------

StackSamplingProfiler::SamplingThread::SamplingThread(
    scoped_ptr<NativeStackSampler> native_sampler,
    const SamplingParams& params,
    CompletedCallback completed_callback)
    : native_sampler_(native_sampler.Pass()),
      params_(params),
      stop_event_(false, false),
      completed_callback_(completed_callback) {
}

StackSamplingProfiler::SamplingThread::~SamplingThread() {}

void StackSamplingProfiler::SamplingThread::ThreadMain() {
  PlatformThread::SetName("Chrome_SamplingProfilerThread");

  CallStackProfiles profiles;
  CollectProfiles(&profiles);
  completed_callback_.Run(profiles);
}

// Depending on how long the sampling takes and the length of the sampling
// interval, a burst of samples could take arbitrarily longer than
// samples_per_burst * sampling_interval. In this case, we (somewhat
// arbitrarily) honor the number of samples requested rather than strictly
// adhering to the sampling intervals. Once we have established users for the
// StackSamplingProfiler and the collected data to judge, we may go the other
// way or make this behavior configurable.
bool StackSamplingProfiler::SamplingThread::CollectProfile(
    CallStackProfile* profile,
    TimeDelta* elapsed_time) {
  ElapsedTimer profile_timer;
  CallStackProfile current_profile;
  native_sampler_->ProfileRecordingStarting(&current_profile.modules);
  current_profile.sampling_period = params_.sampling_interval;
  bool burst_completed = true;
  TimeDelta previous_elapsed_sample_time;
  for (int i = 0; i < params_.samples_per_burst; ++i) {
    if (i != 0) {
      // Always wait, even if for 0 seconds, so we can observe a signal on
      // stop_event_.
      if (stop_event_.TimedWait(
              std::max(params_.sampling_interval - previous_elapsed_sample_time,
                       TimeDelta()))) {
        burst_completed = false;
        break;
      }
    }
    ElapsedTimer sample_timer;
    current_profile.samples.push_back(Sample());
    native_sampler_->RecordStackSample(&current_profile.samples.back());
    previous_elapsed_sample_time = sample_timer.Elapsed();
  }

  *elapsed_time = profile_timer.Elapsed();
  current_profile.profile_duration = *elapsed_time;
  native_sampler_->ProfileRecordingStopped();

  if (burst_completed)
    *profile = current_profile;

  return burst_completed;
}

// In an analogous manner to CollectProfile() and samples exceeding the expected
// total sampling time, bursts may also exceed the burst_interval. We adopt the
// same wait-and-see approach here.
void StackSamplingProfiler::SamplingThread::CollectProfiles(
    CallStackProfiles* profiles) {
  if (stop_event_.TimedWait(params_.initial_delay))
    return;

  TimeDelta previous_elapsed_profile_time;
  for (int i = 0; i < params_.bursts; ++i) {
    if (i != 0) {
      // Always wait, even if for 0 seconds, so we can observe a signal on
      // stop_event_.
      if (stop_event_.TimedWait(
              std::max(params_.burst_interval - previous_elapsed_profile_time,
                       TimeDelta())))
        return;
    }

    CallStackProfile profile;
    if (!CollectProfile(&profile, &previous_elapsed_profile_time))
      return;
    profiles->push_back(profile);
  }
}

void StackSamplingProfiler::SamplingThread::Stop() {
  stop_event_.Signal();
}

// StackSamplingProfiler ------------------------------------------------------

StackSamplingProfiler::SamplingParams::SamplingParams()
    : initial_delay(TimeDelta::FromMilliseconds(0)),
      bursts(1),
      burst_interval(TimeDelta::FromMilliseconds(10000)),
      samples_per_burst(300),
      sampling_interval(TimeDelta::FromMilliseconds(100)),
      preserve_sample_ordering(false) {
}

StackSamplingProfiler::StackSamplingProfiler(PlatformThreadId thread_id,
                                             const SamplingParams& params)
    : thread_id_(thread_id), params_(params) {}

StackSamplingProfiler::~StackSamplingProfiler() {}

void StackSamplingProfiler::Start() {
  scoped_ptr<NativeStackSampler> native_sampler =
      NativeStackSampler::Create(thread_id_);
  if (!native_sampler)
    return;

  sampling_thread_.reset(
      new SamplingThread(
          native_sampler.Pass(), params_,
          (custom_completed_callback_.is_null() ?
           Bind(&PendingProfiles::AppendProfiles,
                Unretained(PendingProfiles::GetInstance())) :
           custom_completed_callback_)));
  if (!PlatformThread::CreateNonJoinable(0, sampling_thread_.get()))
    sampling_thread_.reset();
}

void StackSamplingProfiler::Stop() {
  if (sampling_thread_)
    sampling_thread_->Stop();
}

// static
void StackSamplingProfiler::GetPendingProfiles(CallStackProfiles* profiles) {
  PendingProfiles::GetInstance()->GetAndClearPendingProfiles(profiles);
}

// StackSamplingProfiler::Frame global functions ------------------------------

bool operator==(const StackSamplingProfiler::Frame &a,
                const StackSamplingProfiler::Frame &b) {
  return a.instruction_pointer == b.instruction_pointer &&
      a.module_index == b.module_index;
}

bool operator<(const StackSamplingProfiler::Frame &a,
               const StackSamplingProfiler::Frame &b) {
  return (a.module_index < b.module_index) ||
      (a.module_index == b.module_index &&
       a.instruction_pointer < b.instruction_pointer);
}

}  // namespace base
