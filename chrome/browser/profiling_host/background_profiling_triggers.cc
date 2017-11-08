// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/background_profiling_triggers.h"

#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"

namespace profiling {

namespace {
// Check memory usage every hour. Trigger slow report if needed.
const int kRepeatingCheckMemoryDelayInHours = 1;
const int kSecondReportRepeatingCheckMemoryDelayInHours = 12;

#if defined(OS_ANDROID)
const size_t kBrowserProcessMallocTriggerKb = 100 * 1024;    // 100 MB
const size_t kGPUProcessMallocTriggerKb = 40 * 1024;         // 40 MB
const size_t kRendererProcessMallocTriggerKb = 125 * 1024;   // 125 MB
#else
const size_t kBrowserProcessMallocTriggerKb = 400 * 1024;    // 400 MB
const size_t kGPUProcessMallocTriggerKb = 400 * 1024;        // 400 MB
const size_t kRendererProcessMallocTriggerKb = 500 * 1024;   // 500 MB
#endif  // OS_ANDROID

}  // namespace

BackgroundProfilingTriggers::BackgroundProfilingTriggers(
    ProfilingProcessHost* host)
    : host_(host), weak_ptr_factory_(this) {
  DCHECK(host_);
}

BackgroundProfilingTriggers::~BackgroundProfilingTriggers() {}

void BackgroundProfilingTriggers::StartTimer() {
  // Register a repeating timer to check memory usage periodically.
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromHours(kRepeatingCheckMemoryDelayInHours),
      base::Bind(&BackgroundProfilingTriggers::PerformMemoryUsageChecks,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundProfilingTriggers::PerformMemoryUsageChecks() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &BackgroundProfilingTriggers::PerformMemoryUsageChecksOnIOThread,
          weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundProfilingTriggers::PerformMemoryUsageChecksOnIOThread() {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          base::Bind(&BackgroundProfilingTriggers::OnReceivedMemoryDump,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundProfilingTriggers::OnReceivedMemoryDump(
    bool success,
    memory_instrumentation::mojom::GlobalMemoryDumpPtr dump) {
  if (!success)
    return;

  ProfilingProcessHost::Mode mode = host_->mode();
  for (const auto& proc : dump->process_dumps) {
    bool trigger_report = false;

    if (proc->process_type ==
            memory_instrumentation::mojom::ProcessType::BROWSER &&
        (mode == profiling::ProfilingProcessHost::Mode::kMinimal ||
         mode == profiling::ProfilingProcessHost::Mode::kAll)) {
      trigger_report =
          proc->os_dump->private_footprint_kb > kBrowserProcessMallocTriggerKb;
    }

    if (proc->process_type == memory_instrumentation::mojom::ProcessType::GPU &&
        (mode == profiling::ProfilingProcessHost::Mode::kMinimal ||
         mode == profiling::ProfilingProcessHost::Mode::kAll)) {
      trigger_report =
          proc->os_dump->private_footprint_kb > kGPUProcessMallocTriggerKb;
    }

    if (proc->process_type ==
            memory_instrumentation::mojom::ProcessType::RENDERER &&
        mode == profiling::ProfilingProcessHost::Mode::kAll) {
      trigger_report =
          proc->os_dump->private_footprint_kb > kRendererProcessMallocTriggerKb;
    }

    if (trigger_report)
      TriggerMemoryReportForProcess(proc->pid);
  }
}

void BackgroundProfilingTriggers::TriggerMemoryReportForProcess(
    base::ProcessId pid) {
  host_->RequestProcessReport(pid, "MEMLOG_BACKGROUND_TRIGGER");

  // Reset the timer to avoid uploading too many reports.
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromHours(kSecondReportRepeatingCheckMemoryDelayInHours),
      base::Bind(&BackgroundProfilingTriggers::PerformMemoryUsageChecks,
                 weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace profiling
