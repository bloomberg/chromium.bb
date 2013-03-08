// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

#include <vector>

#include "base/timer.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

class CpuInfoProvider
    : public content::NotificationObserver,
      public SystemInfoProvider<api::experimental_system_info_cpu::CpuInfo> {
 public:
  typedef base::Callback<
      void(scoped_ptr<api::experimental_system_info_cpu::CpuUpdateInfo>)>
          SamplingCallback;

  // Overriden from SystemInfoProvider<CpuInfo>.
  virtual bool QueryInfo(
      api::experimental_system_info_cpu::CpuInfo* info) OVERRIDE;

  // Start sampling the CPU usage. The callback gets called when one sampling
  // cycle is completed periodically with the CPU updated usage info for each
  // processors. It gets called on UI thread, the |callback| gets called on the
  // FILE thread.
  void StartSampling(const SamplingCallback& callback);

  // Stop the sampling cycle. Called on the FILE thread.
  void StopSampling();

  // Return the single shared instance of CpuInfoProvider.
  static CpuInfoProvider* Get();

 private:
  friend class SystemInfoProvider<api::experimental_system_info_cpu::CpuInfo>;
  friend class MockCpuInfoProviderImpl;

  // The amount of time that CPU spent on performing different kinds of work.
  // It is used to calculate the usage percent for processors.
  struct CpuTime {
    CpuTime() : user(0), kernel(0), idle(0) {}
    int64 user;   // user mode.
    int64 kernel; // kernel mode.
    int64 idle;   // twiddling thumbs.
  };

  CpuInfoProvider();

  virtual ~CpuInfoProvider();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Platform specific implementation for querying the CPU time information
  // for each processor.
  virtual bool QueryCpuTimePerProcessor(std::vector<CpuTime>* times);

  // Start and stop sampling on the FILE thread.
  void StartSamplingOnFileThread(const SamplingCallback& callback);
  void StopSamplingOnFileThread();

  // Called when the sampling timer is triggered.
  void DoSample();

  content::NotificationRegistrar registrar_;

  // Indicates whether the CPU sampling is started.
  bool is_sampling_started_;

  // The sampling value returned from the most recent QueryCpuTimePerProcessor
  // call.
  std::vector<CpuTime> baseline_cpu_time_;

  // The callback which will be called when one sampling cycle is completed.
  SamplingCallback callback_;

  // The timer used for polling CPU time periodically. Lives on FILE thread.
  base::RepeatingTimer<CpuInfoProvider>* sampling_timer_;

  // The time interval for sampling, in milliseconds.
  int sampling_interval_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_CPU_CPU_INFO_PROVIDER_H_

