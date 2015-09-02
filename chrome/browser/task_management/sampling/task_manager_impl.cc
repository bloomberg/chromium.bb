// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/sampling/task_manager_impl.h"

#include "base/stl_util.h"
#include "chrome/browser/task_management/providers/browser_process_task_provider.h"
#include "chrome/browser/task_management/providers/child_process_task_provider.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_task_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"

namespace task_management {

namespace {

inline scoped_refptr<base::SequencedTaskRunner> GetBlockingPoolRunner() {
  base::SequencedWorkerPool* blocking_pool =
      content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken token =
      blocking_pool->GetSequenceToken();

  DCHECK(token.IsValid());

  return blocking_pool->GetSequencedTaskRunner(token);
}

base::LazyInstance<TaskManagerImpl> lazy_task_manager_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

TaskManagerImpl::TaskManagerImpl()
    : blocking_pool_runner_(GetBlockingPoolRunner()),
      is_running_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  task_providers_.push_back(new BrowserProcessTaskProvider());
  task_providers_.push_back(new ChildProcessTaskProvider());
  task_providers_.push_back(new WebContentsTaskProvider());

  content::GpuDataManager::GetInstance()->AddObserver(this);
}

TaskManagerImpl::~TaskManagerImpl() {
  content::GpuDataManager::GetInstance()->RemoveObserver(this);

  STLDeleteValues(&task_groups_by_proc_id_);
}

// static
TaskManagerImpl* TaskManagerImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return lazy_task_manager_instance.Pointer();
}

void TaskManagerImpl::ActivateTask(TaskId task_id) {
  GetTaskByTaskId(task_id)->Activate();
}

double TaskManagerImpl::GetCpuUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->cpu_usage();
}

int64 TaskManagerImpl::GetPhysicalMemoryUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->physical_bytes();
}

int64 TaskManagerImpl::GetPrivateMemoryUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->private_bytes();
}

int64 TaskManagerImpl::GetSharedMemoryUsage(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->shared_bytes();
}

int64 TaskManagerImpl::GetGpuMemoryUsage(TaskId task_id,
                                         bool* has_duplicates) const {
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  if (has_duplicates)
    *has_duplicates = task_group->gpu_memory_has_duplicates();
  return task_group->gpu_memory();
}

int TaskManagerImpl::GetIdleWakeupsPerSecond(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->idle_wakeups_per_second();
}

int TaskManagerImpl::GetNaClDebugStubPort(TaskId task_id) const {
#if !defined(DISABLE_NACL)
  return GetTaskGroupByTaskId(task_id)->nacl_debug_stub_port();
#else
  return -1;
#endif  // !defined(DISABLE_NACL)
}

void TaskManagerImpl::GetGDIHandles(TaskId task_id,
                                    int64* current,
                                    int64* peak) const {
#if defined(OS_WIN)
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  *current = task_group->gdi_current_handles();
  *peak = task_group->gdi_peak_handles();
#else
  *current = -1;
  *peak = -1;
#endif  // defined(OS_WIN)
}

void TaskManagerImpl::GetUSERHandles(TaskId task_id,
                                     int64* current,
                                     int64* peak) const {
#if defined(OS_WIN)
  const TaskGroup* task_group = GetTaskGroupByTaskId(task_id);
  *current = task_group->user_current_handles();
  *peak = task_group->user_peak_handles();
#else
  *current = -1;
  *peak = -1;
#endif  // defined(OS_WIN)
}

const base::string16& TaskManagerImpl::GetTitle(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->title();
}

base::string16 TaskManagerImpl::GetProfileName(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetProfileName();
}

const gfx::ImageSkia& TaskManagerImpl::GetIcon(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->icon();
}

const base::ProcessHandle& TaskManagerImpl::GetProcessHandle(
    TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->process_handle();
}

const base::ProcessId& TaskManagerImpl::GetProcessId(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->process_id();
}

Task::Type TaskManagerImpl::GetType(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetType();
}

int64 TaskManagerImpl::GetNetworkUsage(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->network_usage();
}

int64 TaskManagerImpl::GetSqliteMemoryUsed(TaskId task_id) const {
  return GetTaskByTaskId(task_id)->GetSqliteMemoryUsed();
}

bool TaskManagerImpl::GetV8Memory(TaskId task_id,
                                  int64* allocated,
                                  int64* used) const {
  const Task* task = GetTaskByTaskId(task_id);
  if (!task->ReportsV8Memory())
    return false;

  *allocated = task->GetV8MemoryAllocated();
  *used = task->GetV8MemoryUsed();

  return true;
}

bool TaskManagerImpl::GetWebCacheStats(
    TaskId task_id,
    blink::WebCache::ResourceTypeStats* stats) const {
  const Task* task = GetTaskByTaskId(task_id);
  if (!task->ReportsWebCacheStats())
    return false;

  *stats = task->GetWebCacheStats();

  return true;
}

const TaskIdList& TaskManagerImpl::GetTaskIdsList() const {
  DCHECK(is_running_) << "Task manager is not running. You must observe the "
      "task manager for it to start running";

  if (sorted_task_ids_.empty()) {
    sorted_task_ids_.reserve(task_groups_by_task_id_.size());

    // Ensure browser process group of task IDs are at the beginning of the
    // list.
    const TaskGroup* browser_group =
        task_groups_by_proc_id_.at(base::GetCurrentProcId());
    browser_group->AppendSortedTaskIds(&sorted_task_ids_);

    for (const auto& groups_pair : task_groups_by_proc_id_) {
      if (groups_pair.second == browser_group)
        continue;

      groups_pair.second->AppendSortedTaskIds(&sorted_task_ids_);
    }
  }

  return sorted_task_ids_;
}

size_t TaskManagerImpl::GetNumberOfTasksOnSameProcess(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->num_tasks();
}

void TaskManagerImpl::TaskAdded(Task* task) {
  DCHECK(task);

  TaskGroup* task_group = nullptr;
  const base::ProcessId proc_id = task->process_id();
  const TaskId task_id = task->task_id();

  auto itr = task_groups_by_proc_id_.find(proc_id);
  if (itr == task_groups_by_proc_id_.end()) {
    task_group = new TaskGroup(task->process_handle(),
                               proc_id,
                               blocking_pool_runner_);
    task_groups_by_proc_id_[proc_id] = task_group;
  } else {
    task_group = itr->second;
  }

  task_group->AddTask(task);

  task_groups_by_task_id_[task_id] = task_group;

  // Invalidate the cached sorted IDs by clearing the list.
  sorted_task_ids_.clear();

  NotifyObserversOnTaskAdded(task_id);
}

void TaskManagerImpl::TaskRemoved(Task* task) {
  DCHECK(task);

  const base::ProcessId proc_id = task->process_id();
  const TaskId task_id = task->task_id();

  DCHECK(ContainsKey(task_groups_by_proc_id_, proc_id));
  DCHECK(ContainsKey(task_groups_by_task_id_, task_id));

  NotifyObserversOnTaskToBeRemoved(task_id);

  TaskGroup* task_group = task_groups_by_proc_id_.at(proc_id);
  task_group->RemoveTask(task);

  task_groups_by_task_id_.erase(task_id);

  // Invalidate the cached sorted IDs by clearing the list.
  sorted_task_ids_.clear();

  if (task_group->empty()) {
    task_groups_by_proc_id_.erase(proc_id);
    delete task_group;
  }
}

void TaskManagerImpl::OnVideoMemoryUsageStatsUpdate(
    const content::GPUVideoMemoryUsageStats& gpu_memory_stats) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  gpu_memory_stats_ = gpu_memory_stats;
}

// static
void TaskManagerImpl::OnMultipleBytesReadUI(
    std::vector<BytesReadParam>* params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(params);

  for (BytesReadParam& param : *params) {
    if (!GetInstance()->UpdateTasksWithBytesRead(param)) {
      // We can't match a task to the notification.  That might mean the
      // tab that started a download was closed, or the request may have had
      // no originating task associated with it in the first place.
      // We attribute orphaned/unaccounted activity to the Browser process.
      DCHECK(param.origin_pid || (param.child_id != -1));

      param.origin_pid = 0;
      param.child_id = param.route_id = -1;

      GetInstance()->UpdateTasksWithBytesRead(param);
    }
  }
}

void TaskManagerImpl::Refresh() {
  if (IsResourceRefreshEnabled(REFRESH_TYPE_GPU_MEMORY)) {
    content::GpuDataManager::GetInstance()->
        RequestVideoMemoryUsageStatsUpdate();
  }

  for (auto& groups_itr : task_groups_by_proc_id_) {
    groups_itr.second->Refresh(gpu_memory_stats_,
                               GetCurrentRefreshTime(),
                               enabled_resources_flags());
  }

  NotifyObserversOnRefresh(GetTaskIdsList());
}

void TaskManagerImpl::StartUpdating() {
  is_running_ = true;

  for (auto& provider : task_providers_)
    provider->SetObserver(this);

  io_thread_helper_manager_.reset(new IoThreadHelperManager);
}

void TaskManagerImpl::StopUpdating() {
  is_running_ = false;

  io_thread_helper_manager_.reset();

  for (auto& provider : task_providers_)
    provider->ClearObserver();

  STLDeleteValues(&task_groups_by_proc_id_);
  task_groups_by_task_id_.clear();
  sorted_task_ids_.clear();
}

bool TaskManagerImpl::UpdateTasksWithBytesRead(const BytesReadParam& param) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (auto& task_provider : task_providers_) {
    Task* task = task_provider->GetTaskOfUrlRequest(param.origin_pid,
                                                    param.child_id,
                                                    param.route_id);
    if (task) {
      task->OnNetworkBytesRead(param.byte_count);
      return true;
    }
  }

  // Couldn't match the bytes to any existing task.
  return false;
}

TaskGroup* TaskManagerImpl::GetTaskGroupByTaskId(TaskId task_id) const {
  DCHECK(ContainsKey(task_groups_by_task_id_, task_id));

  return task_groups_by_task_id_.at(task_id);
}

Task* TaskManagerImpl::GetTaskByTaskId(TaskId task_id) const {
  return GetTaskGroupByTaskId(task_id)->GetTaskById(task_id);
}

}  // namespace task_management
