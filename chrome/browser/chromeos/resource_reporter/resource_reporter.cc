// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"

#include <cstdint>
#include <queue>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "components/rappor/rappor_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

#define GET_ENUM_VAL(enum_entry) static_cast<int>(enum_entry)

// The task manager refresh interval, currently at 1 minute.
const int64_t kRefreshIntervalSeconds = 60;

// Various memory usage sizes in bytes.
const int64_t kMemory1GB = 1024 * 1024 * 1024;
const int64_t kMemory800MB = 800 * 1024 * 1024;
const int64_t kMemory600MB = 600 * 1024 * 1024;
const int64_t kMemory400MB = 400 * 1024 * 1024;
const int64_t kMemory200MB = 200 * 1024 * 1024;

// The name of the Rappor metric to report the CPU usage.
const char kCpuRapporMetric[] = "ResourceReporter.Cpu";

// The name of the Rappor metric to report the memory usage.
const char kMemoryRapporMetric[] = "ResourceReporter.Memory";

// The name of the string field of the Rappor metrics in which we'll record the
// task's Rappor sample name.
const char kRapporTaskStringField[] = "task";

// The name of the flags field of the Rappor metrics in which we'll store the
// priority of the process on which the task is running.
const char kRapporPriorityFlagsField[] = "priority";

// The name of the flags field of the CPU usage Rappor metrics in which we'll
// record the number of cores in the current system.
const char kRapporNumCoresRangeFlagsField[] = "num_cores_range";

// The name of the flags field of the Rappor metrics in which we'll store the
// CPU / memory usage ranges.
const char kRapporUsageRangeFlagsField[] = "usage_range";

// Currently set to be one day.
const int kMinimumTimeBetweenReportsInMs = 1 * 24 * 60 * 60 * 1000;

// A functor to sort the TaskRecords by their |cpu|.
struct TaskRecordCpuLessThan {
  bool operator()(ResourceReporter::TaskRecord* const& lhs,
                  ResourceReporter::TaskRecord* const& rhs) const {
    if (lhs->cpu_percent == rhs->cpu_percent)
      return lhs->id < rhs->id;
    return lhs->cpu_percent < rhs->cpu_percent;
  }
};

// A functor to sort the TaskRecords by their |memory|.
struct TaskRecordMemoryLessThan {
  bool operator()(ResourceReporter::TaskRecord* const& lhs,
                  ResourceReporter::TaskRecord* const& rhs) const {
    if (lhs->memory_bytes == rhs->memory_bytes)
      return lhs->id < rhs->id;
    return lhs->memory_bytes < rhs->memory_bytes;
  }
};

}  // namespace

ResourceReporter::TaskRecord::TaskRecord(task_management::TaskId task_id)
    : id(task_id), cpu_percent(0.0), memory_bytes(0), is_background(false) {
}

ResourceReporter::TaskRecord::TaskRecord(task_management::TaskId the_id,
                                         const std::string& task_name,
                                         double cpu_percent,
                                         int64_t memory_bytes,
                                         bool background)
    : id(the_id),
      task_name_for_rappor(task_name),
      cpu_percent(cpu_percent),
      memory_bytes(memory_bytes),
      is_background(background) {
}

ResourceReporter::~ResourceReporter() {
}

// static
ResourceReporter* ResourceReporter::GetInstance() {
  return base::Singleton<ResourceReporter>::get();
}

void ResourceReporter::StartMonitoring() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (is_monitoring_)
    return;

  is_monitoring_ = true;
  task_management::TaskManagerInterface::GetTaskManager()->AddObserver(this);
  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&ResourceReporter::OnMemoryPressure, base::Unretained(this))));
}

void ResourceReporter::StopMonitoring() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!is_monitoring_)
    return;

  is_monitoring_ = false;
  memory_pressure_listener_.reset();
  task_management::TaskManagerInterface::GetTaskManager()->RemoveObserver(this);
}

void ResourceReporter::OnTaskAdded(task_management::TaskId id) {
  // Ignore this event.
}

void ResourceReporter::OnTaskToBeRemoved(task_management::TaskId id) {
  auto it = task_records_.find(id);
  if (it == task_records_.end())
    return;

  // Must be erased from the sorted set first.
  // Note: this could mean that the sorted records are now less than
  // |kTopConsumerCount| with other records in |task_records_| that can be
  // added now. That's ok, we ignore this case.
  auto cpu_it = std::find(task_records_by_cpu_.begin(),
                           task_records_by_cpu_.end(),
                           it->second.get());
  if (cpu_it != task_records_by_cpu_.end())
    task_records_by_cpu_.erase(cpu_it);

  auto memory_it = std::find(task_records_by_memory_.begin(),
                              task_records_by_memory_.end(),
                              it->second.get());
  if (memory_it != task_records_by_memory_.end())
    task_records_by_memory_.erase(memory_it);

  task_records_.erase(it);
}

void ResourceReporter::OnTasksRefreshed(
    const task_management::TaskIdList& task_ids) {
  have_seen_first_task_manager_refresh_ = true;

  // A priority queue to sort the task records by their |cpu|. Greatest |cpu|
  // first.
  std::priority_queue<TaskRecord*,
                      std::vector<TaskRecord*>,
                      TaskRecordCpuLessThan> records_by_cpu_queue;
  // A priority queue to sort the task records by their |memory|. Greatest
  // |memory| first.
  std::priority_queue<TaskRecord*,
                      std::vector<TaskRecord*>,
                      TaskRecordMemoryLessThan> records_by_memory_queue;
  task_records_by_cpu_.clear();
  task_records_by_cpu_.reserve(kTopConsumersCount);
  task_records_by_memory_.clear();
  task_records_by_memory_.reserve(kTopConsumersCount);

  for (const auto& id : task_ids) {
    const double cpu_usage = observed_task_manager()->GetCpuUsage(id);
    const int64_t memory_usage =
        observed_task_manager()->GetPhysicalMemoryUsage(id);

    // Browser and GPU processes are reported later using UMA histograms as they
    // don't have any privacy issues.
    const auto task_type = observed_task_manager()->GetType(id);
    switch (task_type) {
      case task_management::Task::UNKNOWN:
      case task_management::Task::ZYGOTE:
        break;

      case task_management::Task::BROWSER:
        last_browser_process_cpu_ = cpu_usage;
        last_browser_process_memory_ = memory_usage >= 0 ? memory_usage : 0;
        break;

      case task_management::Task::GPU:
        last_gpu_process_cpu_ = cpu_usage;
        last_gpu_process_memory_ = memory_usage >= 0 ? memory_usage : 0;
        break;

      default:
        // Other tasks types will be reported using Rappor.
        TaskRecord* task_data = nullptr;
        auto itr = task_records_.find(id);
        if (itr == task_records_.end()) {
          task_data = new TaskRecord(id);
          task_records_[id] = base::WrapUnique(task_data);
        } else {
          task_data = itr->second.get();
        }

        DCHECK_EQ(task_data->id, id);
        task_data->task_name_for_rappor =
            observed_task_manager()->GetTaskNameForRappor(id);
        task_data->cpu_percent = cpu_usage;
        task_data->memory_bytes = memory_usage;
        task_data->is_background =
            observed_task_manager()->IsTaskOnBackgroundedProcess(id);

        // Push only valid or useful data to both priority queues. They might
        // end up having more records than |kTopConsumerCount|, that's fine.
        // We'll take care of that next.
        if (task_data->cpu_percent > 0)
          records_by_cpu_queue.push(task_data);
        if (task_data->memory_bytes > 0)
          records_by_memory_queue.push(task_data);
    }
  }

  // Sort the |kTopConsumersCount| task records by their CPU and memory usage.
  while (!records_by_cpu_queue.empty() &&
         task_records_by_cpu_.size() < kTopConsumersCount) {
    task_records_by_cpu_.push_back(records_by_cpu_queue.top());
    records_by_cpu_queue.pop();
  }

  while (!records_by_memory_queue.empty() &&
         task_records_by_memory_.size() < kTopConsumersCount) {
    task_records_by_memory_.push_back(records_by_memory_queue.top());
    records_by_memory_queue.pop();
  }
}

// static
const size_t ResourceReporter::kTopConsumersCount = 10U;

ResourceReporter::ResourceReporter()
    : TaskManagerObserver(base::TimeDelta::FromSeconds(kRefreshIntervalSeconds),
                          task_management::REFRESH_TYPE_CPU |
                              task_management::REFRESH_TYPE_MEMORY |
                                  task_management::REFRESH_TYPE_PRIORITY),
      system_cpu_cores_range_(GetCurrentSystemCpuCoresRange()) {
}

// static
std::unique_ptr<rappor::Sample> ResourceReporter::CreateRapporSample(
    rappor::RapporService* rappor_service,
    const ResourceReporter::TaskRecord& task_record) {
  std::unique_ptr<rappor::Sample> sample(
      rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE));
  sample->SetStringField(kRapporTaskStringField,
                         task_record.task_name_for_rappor);
  sample->SetFlagsField(kRapporPriorityFlagsField,
                        task_record.is_background ?
                            GET_ENUM_VAL(TaskProcessPriority::BACKGROUND) :
                            GET_ENUM_VAL(TaskProcessPriority::FOREGROUND),
                        GET_ENUM_VAL(TaskProcessPriority::NUM_PRIORITIES));
  return sample;
}

// static
ResourceReporter::CpuUsageRange
ResourceReporter::GetCpuUsageRange(double cpu) {
  if (cpu > 60.0)
    return CpuUsageRange::RANGE_ABOVE_60_PERCENT;
  if (cpu > 30.0)
    return CpuUsageRange::RANGE_30_TO_60_PERCENT;
  if (cpu > 10.0)
    return CpuUsageRange::RANGE_10_TO_30_PERCENT;

  return CpuUsageRange::RANGE_0_TO_10_PERCENT;
}

// static
ResourceReporter::MemoryUsageRange
ResourceReporter::GetMemoryUsageRange(int64_t memory_in_bytes) {
  if (memory_in_bytes > kMemory1GB)
    return MemoryUsageRange::RANGE_ABOVE_1_GB;
  if (memory_in_bytes > kMemory800MB)
    return MemoryUsageRange::RANGE_800_TO_1_GB;
  if (memory_in_bytes > kMemory600MB)
    return MemoryUsageRange::RANGE_600_TO_800_MB;
  if (memory_in_bytes > kMemory400MB)
      return MemoryUsageRange::RANGE_400_TO_600_MB;
  if (memory_in_bytes > kMemory200MB)
      return MemoryUsageRange::RANGE_200_TO_400_MB;

  return MemoryUsageRange::RANGE_0_TO_200_MB;
}

// static
ResourceReporter::CpuCoresNumberRange
ResourceReporter::GetCurrentSystemCpuCoresRange() {
  const int cpus = base::SysInfo::NumberOfProcessors();

  if (cpus > 16)
    return CpuCoresNumberRange::RANGE_ABOVE_16_CORES;
  if (cpus > 8)
    return CpuCoresNumberRange::RANGE_9_TO_16_CORES;
  if (cpus > 4)
    return CpuCoresNumberRange::RANGE_5_TO_8_CORES;
  if (cpus > 2)
    return CpuCoresNumberRange::RANGE_3_TO_4_CORES;
  if (cpus == 2)
    return CpuCoresNumberRange::RANGE_2_CORES;
  if (cpus == 1)
    return CpuCoresNumberRange::RANGE_1_CORE;

  NOTREACHED();
  return CpuCoresNumberRange::RANGE_NA;
}

const ResourceReporter::TaskRecord* ResourceReporter::SampleTaskByCpu() const {
  // Perform a weighted random sampling taking the tasks' CPU usage as their
  // weights to randomly select one of them to be reported by Rappor. The higher
  // the CPU usage, the higher the chance that the task will be selected.
  // See https://en.wikipedia.org/wiki/Reservoir_sampling.
  TaskRecord* sampled_task = nullptr;
  double cpu_weights_sum = 0;
  for (const auto& task_data : task_records_by_cpu_) {
    if ((base::RandDouble() * (cpu_weights_sum + task_data->cpu_percent)) >=
        cpu_weights_sum) {
      sampled_task = task_data;
    }
    cpu_weights_sum += task_data->cpu_percent;
  }

  return sampled_task;
}

const ResourceReporter::TaskRecord*
ResourceReporter::SampleTaskByMemory() const {
  // Perform a weighted random sampling taking the tasks' memory usage as their
  // weights to randomly select one of them to be reported by Rappor. The higher
  // the memory usage, the higher the chance that the task will be selected.
  // See https://en.wikipedia.org/wiki/Reservoir_sampling.
  TaskRecord* sampled_task = nullptr;
  int64_t memory_weights_sum = 0;
  for (const auto& task_data : task_records_by_memory_) {
    if ((base::RandDouble() * (memory_weights_sum + task_data->memory_bytes)) >=
        memory_weights_sum) {
      sampled_task = task_data;
    }
    memory_weights_sum += task_data->memory_bytes;
  }

  return sampled_task;
}

void ResourceReporter::OnMemoryPressure(
    MemoryPressureLevel memory_pressure_level) {
  if (have_seen_first_task_manager_refresh_ &&
      memory_pressure_level ==
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    // Report browser and GPU processes usage using UMA histograms.
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceReporter.BrowserProcess.CpuUsage",
        GET_ENUM_VAL(GetCpuUsageRange(last_browser_process_cpu_)),
        GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceReporter.BrowserProcess.MemoryUsage",
        GET_ENUM_VAL(GetMemoryUsageRange(last_browser_process_memory_)),
        GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceReporter.GpuProcess.CpuUsage",
        GET_ENUM_VAL(GetCpuUsageRange(last_gpu_process_cpu_)),
        GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceReporter.GpuProcess.MemoryUsage",
        GET_ENUM_VAL(GetMemoryUsageRange(last_gpu_process_memory_)),
        GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));

    // For the rest of tasks, report them using Rappor.
    auto rappor_service = g_browser_process->rappor_service();
    if (!rappor_service)
      return;

    // We only record Rappor samples only if it's the first ever critical memory
    // pressure event we receive, or it has been more than
    // |kMinimumTimeBetweenReportsInMs| since the last time we recorded samples.
    if (!have_seen_first_memory_pressure_event_) {
      have_seen_first_memory_pressure_event_ = true;
    } else if ((base::TimeTicks::Now() - last_memory_pressure_event_time_) <
        base::TimeDelta::FromMilliseconds(kMinimumTimeBetweenReportsInMs)) {
      return;
    }

    last_memory_pressure_event_time_ = base::TimeTicks::Now();

    // Use weighted random sampling to select a task to report in the CPU
    // metric.
    const TaskRecord* sampled_cpu_task = SampleTaskByCpu();
    if (sampled_cpu_task) {
      std::unique_ptr<rappor::Sample> cpu_sample(
          CreateRapporSample(rappor_service, *sampled_cpu_task));
      cpu_sample->SetFlagsField(kRapporNumCoresRangeFlagsField,
                                GET_ENUM_VAL(system_cpu_cores_range_),
                                GET_ENUM_VAL(CpuCoresNumberRange::NUM_RANGES));
      cpu_sample->SetFlagsField(
          kRapporUsageRangeFlagsField,
          GET_ENUM_VAL(GetCpuUsageRange(sampled_cpu_task->cpu_percent)),
          GET_ENUM_VAL(CpuUsageRange::NUM_RANGES));
      rappor_service->RecordSampleObj(kCpuRapporMetric, std::move(cpu_sample));
    }

    // Use weighted random sampling to select a task to report in the memory
    // metric.
    const TaskRecord* sampled_memory_task = SampleTaskByMemory();
    if (sampled_memory_task) {
      std::unique_ptr<rappor::Sample> memory_sample(
          CreateRapporSample(rappor_service, *sampled_memory_task));
      memory_sample->SetFlagsField(
          kRapporUsageRangeFlagsField,
          GET_ENUM_VAL(GetMemoryUsageRange(sampled_memory_task->memory_bytes)),
          GET_ENUM_VAL(MemoryUsageRange::NUM_RANGES));
      rappor_service->RecordSampleObj(kMemoryRapporMetric,
                                      std::move(memory_sample));
    }
  }
}

}  // namespace chromeos
