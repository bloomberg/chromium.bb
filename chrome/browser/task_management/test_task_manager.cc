// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_management/test_task_manager.h"

namespace task_management {

TestTaskManager::TestTaskManager()
    : handle_(base::kNullProcessHandle),
      pid_(base::kNullProcessId) {
  set_timer_for_testing(scoped_ptr<base::Timer>(new base::MockTimer(true,
                                                                    true)));
}

TestTaskManager::~TestTaskManager() {
}

// task_management::TaskManagerInterface:
void TestTaskManager::ActivateTask(TaskId task_id) {
}

double TestTaskManager::GetCpuUsage(TaskId task_id) const {
  return 0.0;
}

int64 TestTaskManager::GetPhysicalMemoryUsage(TaskId task_id) const {
  return -1;
}

int64 TestTaskManager::GetPrivateMemoryUsage(TaskId task_id) const {
  return -1;
}

int64 TestTaskManager::GetSharedMemoryUsage(TaskId task_id) const {
  return -1;
}

int64 TestTaskManager::GetGpuMemoryUsage(TaskId task_id,
                                         bool* has_duplicates) const {
  return -1;
}

int TestTaskManager::GetIdleWakeupsPerSecond(TaskId task_id) const {
  return -1;
}

int TestTaskManager::GetNaClDebugStubPort(TaskId task_id) const {
  return -1;
}

void TestTaskManager::GetGDIHandles(TaskId task_id,
                                    int64* current,
                                    int64* peak) const {
}

void TestTaskManager::GetUSERHandles(TaskId task_id,
                                     int64* current,
                                     int64* peak) const {
}

int TestTaskManager::GetOpenFdCount(TaskId task_id) const {
  return -1;
}

bool TestTaskManager::IsTaskOnBackgroundedProcess(TaskId task_id) const {
  return false;
}

const base::string16& TestTaskManager::GetTitle(TaskId task_id) const {
  return title_;
}

const std::string& TestTaskManager::GetTaskNameForRappor(TaskId task_id) const {
  return rappor_sample_;
}

base::string16 TestTaskManager::GetProfileName(TaskId task_id) const {
  return base::string16();
}

const gfx::ImageSkia& TestTaskManager::GetIcon(TaskId task_id) const {
  return icon_;
}

const base::ProcessHandle& TestTaskManager::GetProcessHandle(
    TaskId task_id) const {
  return handle_;
}

const base::ProcessId& TestTaskManager::GetProcessId(TaskId task_id) const {
  return pid_;
}

Task::Type TestTaskManager::GetType(TaskId task_id) const {
  return Task::UNKNOWN;
}

int64 TestTaskManager::GetNetworkUsage(TaskId task_id) const {
  return -1;
}

int64 TestTaskManager::GetProcessTotalNetworkUsage(TaskId task_id) const {
  return -1;
}

int64 TestTaskManager::GetSqliteMemoryUsed(TaskId task_id) const {
  return -1;
}

bool TestTaskManager::GetV8Memory(TaskId task_id,
                                  int64* allocated,
                                  int64* used) const {
  return false;
}

bool TestTaskManager::GetWebCacheStats(
    TaskId task_id,
    blink::WebCache::ResourceTypeStats* stats) const {
  return false;
}

const TaskIdList& TestTaskManager::GetTaskIdsList() const {
  return ids_;
}

size_t TestTaskManager::GetNumberOfTasksOnSameProcess(TaskId task_id) const {
  return 1;
}

base::TimeDelta TestTaskManager::GetRefreshTime() {
  return GetCurrentRefreshTime();
}

int64 TestTaskManager::GetEnabledFlags() {
  return enabled_resources_flags();
}

}  // namespace task_management
