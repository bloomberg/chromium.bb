// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/cpu_profiler.h"

#include <stddef.h>

#include "base/debug/stack_trace.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"

namespace base {

namespace {

const char kMode[] = "Mode";
const char kInitialDelay[] = "InitialDelay";
const char kNumberOfBursts[] = "NumberOfBursts";
const char kBurstIdleTime[] = "IdleTime";
const char kNumberOfSamples[] = "NumberOfSamples";
const char kSamplingSleepTime[] = "SamplingSleepTime";

}  // namespace

class CpuProfiler::SamplingThread : public PlatformThread::Delegate {
 public:
  SamplingThread(CpuProfiler* cpu_profiler);
  ~SamplingThread() override;

  // Implementation of PlatformThread::Delegate:
  void ThreadMain() override;

  void Stop();

  void GetSamples();

 private:
  CpuProfiler* cpu_profiler_;

  bool thread_running_;

  base::WaitableEvent waitable_event_;

  DISALLOW_COPY_AND_ASSIGN(SamplingThread);
};

CpuProfiler::SamplingThread::SamplingThread(CpuProfiler* cpu_profiler)
    : cpu_profiler_(cpu_profiler),
      thread_running_(false),
      waitable_event_(false, false) {
}

CpuProfiler::SamplingThread::~SamplingThread() {}

void CpuProfiler::SamplingThread::ThreadMain() {
  PlatformThread::SetName("Chrome_CPUProfilerThread");
  thread_running_ = true;

  if (cpu_profiler_->GetStringParam(kMode) == "bursts") {
    int64 initial_delay = cpu_profiler_->GetInt64Param(kInitialDelay);
    int number_of_bursts = cpu_profiler_->GetIntParam(kNumberOfBursts);
    int64 burst_idle_time = cpu_profiler_->GetInt64Param(kBurstIdleTime);
    int number_of_samples = cpu_profiler_->GetIntParam(kNumberOfSamples);
    int64 sampling_sleep_time =
        cpu_profiler_->GetInt64Param(kSamplingSleepTime);

    if (waitable_event_.TimedWait(TimeDelta::FromMicroseconds(initial_delay)))
      return;
    for (int i = 0; i < number_of_bursts; ++i) {
      int64 delta = 0;
      for (int j = 0; ; ++j) {
        base::ElapsedTimer time;
        GetSamples();
        delta = time.Elapsed().InMicroseconds();
        if (j == (number_of_samples - 1))
          break;
        if (delta < sampling_sleep_time) {
          if (waitable_event_.TimedWait(
              TimeDelta::FromMicroseconds(sampling_sleep_time - delta)))
            return;
        } else {
          if (waitable_event_.TimedWait(
              TimeDelta::FromMicroseconds(sampling_sleep_time)))
            return;
        }
      }
      if (waitable_event_.TimedWait(
          TimeDelta::FromMicroseconds(burst_idle_time - delta)))
        return;
    }
  }
}

void CpuProfiler::SamplingThread::Stop() {
  waitable_event_.Signal();
}

void CpuProfiler::SamplingThread::GetSamples() {
  cpu_profiler_->OnTimer();
}

void CpuProfiler::SamplingThreadDeleter::operator()(
    SamplingThread* thread) const {
  delete thread;
}

void CpuProfiler::Initialize(const std::map<std::string, std::string>* params) {
  if (!CpuProfiler::IsPlatformSupported())
    return;

  std::map<std::string, std::string> default_params;
  default_params[kMode] = "bursts";
  default_params[kInitialDelay] = "0"; // 0 sec
  default_params[kNumberOfBursts] = "1";
  default_params[kBurstIdleTime] = "10000000"; // 10 sec
  default_params[kNumberOfSamples] = "300";
  default_params[kSamplingSleepTime] = "100000"; // 100 ms

  if (!params)
    SetParams(default_params);
  else
    SetParams(*params);

  sampling_thread_.reset(new SamplingThread(this));
  if (!PlatformThread::Create(0, sampling_thread_.get(),
                              &sampling_thread_handle_)) {
    LOG(ERROR) << "failed to create thread";
  }
}

void CpuProfiler::Stop() {
  if (sampling_thread_)
    sampling_thread_->Stop();
}


std::string CpuProfiler::GetStringParam(const std::string& key) const{
  const auto entry = params_.find(key);
  if (entry != params_.end()) {
    return entry->second;
  }
  return "";
}

int CpuProfiler::GetIntParam(const std::string& key) const {
  const auto entry = params_.find(key);
  if (entry != params_.end()) {
    int output;
    if (base::StringToInt(entry->second, &output))
      return output;
  }
  return 0;
}

int64 CpuProfiler::GetInt64Param(const std::string& key) const {
  const auto entry = params_.find(key);
  if (entry != params_.end()) {
    int64 output;
    if (base::StringToInt64(entry->second, &output))
      return output;
  }
  return 0;
}

void CpuProfiler::SetParams(
    const std::map<std::string, std::string>& params) {
  params_ = params;
}

}  // namespace base
