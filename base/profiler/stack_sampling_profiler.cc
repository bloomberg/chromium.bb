// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/elapsed_timer.h"

template <typename T> struct DefaultSingletonTraits;

namespace base {

namespace {

// Thread-safe singleton class that stores collected profiles waiting to be
// processed.
class PendingProfiles {
 public:
  PendingProfiles();
  ~PendingProfiles();

  static PendingProfiles* GetInstance();

  // Appends |profiles|. This function is thread safe.
  void PutProfiles(const std::vector<StackSamplingProfiler::Profile>& profiles);
  // Gets the pending profiles into *|profiles|. This function is thread safe.
  void GetProfiles(std::vector<StackSamplingProfiler::Profile>* profiles);

 private:
  Lock profiles_lock_;
  std::vector<StackSamplingProfiler::Profile> profiles_;

  DISALLOW_COPY_AND_ASSIGN(PendingProfiles);
};

PendingProfiles::PendingProfiles() {}

PendingProfiles::~PendingProfiles() {}

// static
PendingProfiles* PendingProfiles::GetInstance() {
  return Singleton<PendingProfiles>::get();
}

void PendingProfiles::PutProfiles(
    const std::vector<StackSamplingProfiler::Profile>& profiles) {
  AutoLock scoped_lock(profiles_lock_);
  profiles_.insert(profiles_.end(), profiles.begin(), profiles.end());
}

void PendingProfiles::GetProfiles(
    std::vector<StackSamplingProfiler::Profile>* profiles) {
  profiles->clear();

  AutoLock scoped_lock(profiles_lock_);
  profiles_.swap(*profiles);
}
}  // namespace

StackSamplingProfiler::Module::Module() : base_address(nullptr) {}
StackSamplingProfiler::Module::Module(const void* base_address,
                                      const std::string& id,
                                      const FilePath& filename)
    : base_address(base_address), id(id), filename(filename) {}

StackSamplingProfiler::Module::~Module() {}

StackSamplingProfiler::Frame::Frame()
    : instruction_pointer(nullptr),
      module_index(-1) {}

StackSamplingProfiler::Frame::Frame(const void* instruction_pointer,
                                    int module_index)
    : instruction_pointer(instruction_pointer),
      module_index(module_index) {}

StackSamplingProfiler::Frame::~Frame() {}

StackSamplingProfiler::Profile::Profile() : preserve_sample_ordering(false) {}

StackSamplingProfiler::Profile::~Profile() {}

class StackSamplingProfiler::SamplingThread : public PlatformThread::Delegate {
 public:
  // Samples stacks using |native_sampler|. When complete, invokes
  // |profiles_callback| with the collected profiles. |profiles_callback| must
  // be thread-safe and may consume the contents of the vector.
  SamplingThread(
      scoped_ptr<NativeStackSampler> native_sampler,
      const SamplingParams& params,
      Callback<void(const std::vector<Profile>&)> completed_callback);
  ~SamplingThread() override;

  // Implementation of PlatformThread::Delegate:
  void ThreadMain() override;

  void Stop();

 private:
  // Collects a profile from a single burst. Returns true if the profile was
  // collected, or false if collection was stopped before it completed.
  bool CollectProfile(Profile* profile, TimeDelta* elapsed_time);
  // Collects profiles from all bursts, or until the sampling is stopped. If
  // stopped before complete, |profiles| will contains only full bursts.
  void CollectProfiles(std::vector<Profile>* profiles);

  scoped_ptr<NativeStackSampler> native_sampler_;

  const SamplingParams params_;

  WaitableEvent stop_event_;

  Callback<void(const std::vector<Profile>&)> completed_callback_;

  DISALLOW_COPY_AND_ASSIGN(SamplingThread);
};

StackSamplingProfiler::SamplingThread::SamplingThread(
    scoped_ptr<NativeStackSampler> native_sampler,
    const SamplingParams& params,
    Callback<void(const std::vector<Profile>&)> completed_callback)
    : native_sampler_(native_sampler.Pass()),
      params_(params),
      stop_event_(false, false),
      completed_callback_(completed_callback) {
}

StackSamplingProfiler::SamplingThread::~SamplingThread() {}

void StackSamplingProfiler::SamplingThread::ThreadMain() {
  PlatformThread::SetName("Chrome_SamplingProfilerThread");

  std::vector<Profile> profiles;
  CollectProfiles(&profiles);
  completed_callback_.Run(profiles);
}

bool StackSamplingProfiler::SamplingThread::CollectProfile(
    Profile* profile,
    TimeDelta* elapsed_time) {
  ElapsedTimer profile_timer;
  Profile current_profile;
  native_sampler_->ProfileRecordingStarting(&current_profile);
  current_profile.sampling_period = params_.sampling_interval;
  bool stopped_early = false;
  for (int i = 0; i < params_.samples_per_burst; ++i) {
    ElapsedTimer sample_timer;
    current_profile.samples.push_back(Sample());
    native_sampler_->RecordStackSample(&current_profile.samples.back());
    TimeDelta elapsed_sample_time = sample_timer.Elapsed();
    if (i != params_.samples_per_burst - 1) {
      if (stop_event_.TimedWait(
              std::max(params_.sampling_interval - elapsed_sample_time,
                       TimeDelta()))) {
        stopped_early = true;
        break;
      }
    }
  }

  *elapsed_time = profile_timer.Elapsed();
  current_profile.profile_duration = *elapsed_time;
  native_sampler_->ProfileRecordingStopped();

  if (!stopped_early)
    *profile = current_profile;

  return !stopped_early;
}

void StackSamplingProfiler::SamplingThread::CollectProfiles(
    std::vector<Profile>* profiles) {
  if (stop_event_.TimedWait(params_.initial_delay))
    return;

  for (int i = 0; i < params_.bursts; ++i) {
    Profile profile;
    TimeDelta elapsed_profile_time;
    if (CollectProfile(&profile, &elapsed_profile_time))
      profiles->push_back(profile);
    else
      return;

    if (stop_event_.TimedWait(
            std::max(params_.burst_interval - elapsed_profile_time,
                     TimeDelta())))
      return;
  }
}

void StackSamplingProfiler::SamplingThread::Stop() {
  stop_event_.Signal();
}

void StackSamplingProfiler::SamplingThreadDeleter::operator()(
    SamplingThread* thread) const {
  delete thread;
}

StackSamplingProfiler::NativeStackSampler::NativeStackSampler() {}

StackSamplingProfiler::NativeStackSampler::~NativeStackSampler() {}

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
  native_sampler_ = NativeStackSampler::Create(thread_id_);
  if (!native_sampler_)
    return;

  sampling_thread_.reset(
      new SamplingThread(
          native_sampler_.Pass(), params_,
          (custom_completed_callback_.is_null() ?
           Bind(&PendingProfiles::PutProfiles,
                Unretained(PendingProfiles::GetInstance())) :
           custom_completed_callback_)));
  if (!PlatformThread::CreateNonJoinable(0, sampling_thread_.get()))
    LOG(ERROR) << "failed to create thread";
}

void StackSamplingProfiler::Stop() {
  if (sampling_thread_)
    sampling_thread_->Stop();
}

// static
void StackSamplingProfiler::GetPendingProfiles(std::vector<Profile>* profiles) {
  PendingProfiles::GetInstance()->GetProfiles(profiles);
}

void StackSamplingProfiler::SetCustomCompletedCallback(
    Callback<void(const std::vector<Profile>&)> callback) {
  custom_completed_callback_ = callback;
}

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
