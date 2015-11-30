// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGEMENT_TEST_TASK_MANAGER_H_
#define CHROME_BROWSER_TASK_MANAGEMENT_TEST_TASK_MANAGER_H_

#include "base/macros.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/task_management/task_manager_interface.h"

namespace task_management {

// This is a partial stub implementation that can be used as a base class for
// implementations of the task manager used in unit tests.
class TestTaskManager : public TaskManagerInterface {
 public:
  TestTaskManager();
  ~TestTaskManager() override;

  // task_management::TaskManagerInterface:
  void ActivateTask(TaskId task_id) override;
  double GetCpuUsage(TaskId task_id) const override;
  int64 GetPhysicalMemoryUsage(TaskId task_id) const override;
  int64 GetPrivateMemoryUsage(TaskId task_id) const override;
  int64 GetSharedMemoryUsage(TaskId task_id) const override;
  int64 GetGpuMemoryUsage(TaskId task_id, bool* has_duplicates) const override;
  int GetIdleWakeupsPerSecond(TaskId task_id) const override;
  int GetNaClDebugStubPort(TaskId task_id) const override;
  void GetGDIHandles(TaskId task_id,
                     int64* current,
                     int64* peak) const override;
  void GetUSERHandles(TaskId task_id,
                      int64* current,
                      int64* peak) const override;
  bool IsTaskOnBackgroundedProcess(TaskId task_id) const override;
  const base::string16& GetTitle(TaskId task_id) const override;
  const std::string& GetTaskNameForRappor(TaskId task_id) const override;
  base::string16 GetProfileName(TaskId task_id) const override;
  const gfx::ImageSkia& GetIcon(TaskId task_id) const override;
  const base::ProcessHandle& GetProcessHandle(TaskId task_id) const override;
  const base::ProcessId& GetProcessId(TaskId task_id) const override;
  Task::Type GetType(TaskId task_id) const override;
  int64 GetNetworkUsage(TaskId task_id) const override;
  int64 GetProcessTotalNetworkUsage(TaskId task_id) const override;
  int64 GetSqliteMemoryUsed(TaskId task_id) const override;
  bool GetV8Memory(TaskId task_id,
                   int64* allocated,
                   int64* used) const override;
  bool GetWebCacheStats(
      TaskId task_id,
      blink::WebCache::ResourceTypeStats* stats) const override;
  const TaskIdList& GetTaskIdsList() const override;
  size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const override;

  base::TimeDelta GetRefreshTime();
  int64 GetEnabledFlags();

 protected:
  // task_management::TaskManager:
  void Refresh() override {}
  void StartUpdating() override {}
  void StopUpdating() override {}

  base::ProcessHandle handle_;
  base::ProcessId pid_;
  base::string16 title_;
  std::string rappor_sample_;
  gfx::ImageSkia icon_;
  TaskIdList ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestTaskManager);
};

}  // namespace task_management

#endif  // CHROME_BROWSER_TASK_MANAGEMENT_TEST_TASK_MANAGER_H_
