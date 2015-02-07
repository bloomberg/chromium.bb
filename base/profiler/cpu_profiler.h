// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_CPU_PROFILER_H_
#define BASE_PROFILER_CPU_PROFILER_H_

#include <map>

#include "base/base_export.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "base/timer/timer.h"

namespace base {

#if defined(OS_WIN)
struct StackTraceEntry {
  DWORD64 rsp;
  DWORD64 rip;
  HMODULE module;
};
#endif

// CpuProfiler is a prototype of a technique that periodically stops the main
// thread to samples its stack. It does not do anything useful with the
// collected information yet; this implementation is for the purposes of
// determining perf impact of the profiler itself.
//
// Currently, this is intended to sample the main thread. It should be allocated
// on that thread, then Initialize should be called to start the sampling, and
// Stop called to stop the sampling.
class BASE_EXPORT CpuProfiler {
 public:
  CpuProfiler();
  ~CpuProfiler();

  void Initialize(const std::map<std::string, std::string>* params);
  void Stop();

  std::string GetStringParam(const std::string& key) const;
  int GetIntParam(const std::string& key) const;
  int64 GetInt64Param(const std::string& key) const;

 private:
  class SamplingThread;
  struct SamplingThreadDeleter {
    void operator() (SamplingThread* thread) const;
  };

  static bool IsPlatformSupported();

  void SetParams(const std::map<std::string, std::string>& params);

  void OnTimer();

#if defined(OS_WIN)
  void ProcessStack(StackTraceEntry* stack, int stack_depth);

  HANDLE main_thread_;
  int main_thread_stack_depth_;
  StackTraceEntry main_thread_stack_[64];

  std::map<HMODULE, base::string16> modules_;
#endif

  std::map<std::string, std::string> params_;

  scoped_ptr<SamplingThread, SamplingThreadDeleter> sampling_thread_;
  PlatformThreadHandle sampling_thread_handle_;

  DISALLOW_COPY_AND_ASSIGN(CpuProfiler);
};

}  // namespace base

#endif   // BASE_PROFILER_CPU_PROFILER_H_
