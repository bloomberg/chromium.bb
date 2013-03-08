// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"

#include "base/sys_info.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuInfo;
using api::experimental_system_info_cpu::CpuUpdateInfo;
using content::BrowserThread;

// Default sampling interval is 1000ms.
const unsigned int kDefaultSamplingIntervalMs = 1000;

CpuInfoProvider::CpuInfoProvider()
  : is_sampling_started_(false),
    sampling_timer_(NULL),
    sampling_interval_(kDefaultSamplingIntervalMs) {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

CpuInfoProvider::~CpuInfoProvider() {
  DCHECK(sampling_timer_ == NULL);
  registrar_.RemoveAll();
}

bool CpuInfoProvider::QueryInfo(CpuInfo* info) {
  if (info == NULL)
    return false;

  info->num_of_processors = base::SysInfo::NumberOfProcessors();
  info->arch_name = base::SysInfo::OperatingSystemArchitecture();
  info->model_name = base::SysInfo::CPUModelName();
  return true;
}

void CpuInfoProvider::StartSampling(const SamplingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CpuInfoProvider::StartSamplingOnFileThread, this, callback));
}

void CpuInfoProvider::StopSampling() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CpuInfoProvider::StopSamplingOnFileThread, this));
}

// static
CpuInfoProvider* CpuInfoProvider::Get() {
  return CpuInfoProvider::GetInstance<CpuInfoProvider>();
}

void CpuInfoProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      StopSampling();
      break;
    default:
      NOTREACHED();
      break;
  }
}

void CpuInfoProvider::StartSamplingOnFileThread(
    const SamplingCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (is_sampling_started_)
    return;

  if (!QueryCpuTimePerProcessor(&baseline_cpu_time_))
    return;

  is_sampling_started_ = true;
  callback_ = callback;
  DCHECK(!sampling_timer_) << "The sampling timer has leaked. Did you forgot "
                              "to call StopSampling?";
  // Should be deleted on FILE thread.
  sampling_timer_ = new base::RepeatingTimer<CpuInfoProvider>();
  sampling_timer_->Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(sampling_interval_),
      this, &CpuInfoProvider::DoSample);
}

void CpuInfoProvider::StopSamplingOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (!is_sampling_started_) return;
  delete sampling_timer_;
  sampling_timer_ = NULL;
  baseline_cpu_time_.clear();
  is_sampling_started_ = false;
}

void CpuInfoProvider::DoSample() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<CpuTime> next_cpu_time;
  if (!QueryCpuTimePerProcessor(&next_cpu_time))
    return;

  // Calculate the CPU usage information from the next_cpu_time and
  // |baseline_cpu_time_|.
  double total_usage = 0;
  scoped_ptr<CpuUpdateInfo> info(new CpuUpdateInfo());
  for (size_t i = 0; i < next_cpu_time.size(); ++i) {
    double total_time =
        (next_cpu_time[i].user - baseline_cpu_time_[i].user) +
        (next_cpu_time[i].kernel - baseline_cpu_time_[i].kernel) +
        (next_cpu_time[i].idle - baseline_cpu_time_[i].idle);
    double idle_time = next_cpu_time[i].idle - baseline_cpu_time_[i].idle;

    double usage = 0;
    if (total_time != 0)
      usage = (100 - idle_time * 100 / total_time);
    info->usage_per_processor.push_back(usage);
    total_usage += usage;
  }

  info->average_usage = total_usage / next_cpu_time.size();
  if (!callback_.is_null())
    callback_.Run(info.Pass());
  // Use next_cpu_time as baseline_cpu_time_ for the next sampling cycle.
  baseline_cpu_time_.swap(next_cpu_time);
}

}  // namespace extensions
