// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/mock_timer.h"
#include "chrome/browser/task_management/task_manager_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_management {

namespace {

// This is a partial stub implementation to test the behavior of the base class
// TaskManagerInterface in response to adding and removing observers.
class TestTaskManager : public TaskManagerInterface {
 public:
  TestTaskManager()
      : TaskManagerInterface(),
        handle_(),
        id_(),
        title_(),
        icon_(),
        ids_() {
    set_timer_for_testing(scoped_ptr<base::Timer>(new base::MockTimer(true,
                                                                      true)));
  }
  ~TestTaskManager() override {}

  // task_management::TaskManagerInterface:
  void ActivateTask(TaskId task_id) override {}
  double GetCpuUsage(TaskId task_id) const override { return 0.0; }
  int64 GetPhysicalMemoryUsage(TaskId task_id) const override { return -1; }
  int64 GetPrivateMemoryUsage(TaskId task_id) const override { return -1; }
  int64 GetSharedMemoryUsage(TaskId task_id) const override { return -1; }
  int64 GetGpuMemoryUsage(TaskId task_id, bool* has_duplicates) const override {
    return -1;
  }
  int GetIdleWakeupsPerSecond(TaskId task_id) const override { return -1; }
  int GetNaClDebugStubPort(TaskId task_id) const override { return -1; }
  void GetGDIHandles(TaskId task_id,
                     int64* current,
                     int64* peak) const override {}
  void GetUSERHandles(TaskId task_id,
                      int64* current,
                      int64* peak) const override {}
  const base::string16& GetTitle(TaskId task_id) const override {
    return title_;
  }
  base::string16 GetProfileName(TaskId task_id) const override {
    return base::string16();
  }
  const gfx::ImageSkia& GetIcon(TaskId task_id) const override { return icon_; }
  const base::ProcessHandle& GetProcessHandle(TaskId task_id) const override {
    return handle_;
  }
  const base::ProcessId& GetProcessId(TaskId task_id) const override {
    return id_;
  }
  Task::Type GetType(TaskId task_id) const override { return Task::UNKNOWN; }
  int64 GetNetworkUsage(TaskId task_id) const override { return -1; }
  int64 GetSqliteMemoryUsed(TaskId task_id) const override { return -1; }
  bool GetV8Memory(TaskId task_id,
                   int64* allocated,
                   int64* used) const override { return false; }
  bool GetWebCacheStats(
      TaskId task_id,
      blink::WebCache::ResourceTypeStats* stats) const override {
    return false;
  }
  const TaskIdList& GetTaskIdsList() const override {
    return ids_;
  }
  size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const override {
    return 1;
  }

  base::TimeDelta GetRefreshTime() {
    return GetCurrentRefreshTime();
  }

  int64 GetEnabledFlags() {
    return enabled_resources_flags();
  }

 protected:
  // task_management::TaskManager:
  void Refresh() override {}
  void StartUpdating() override {}
  void StopUpdating() override {}

 private:
  base::ProcessHandle handle_;
  base::ProcessId id_;
  base::string16 title_;
  gfx::ImageSkia icon_;
  TaskIdList ids_;

  DISALLOW_COPY_AND_ASSIGN(TestTaskManager);
};

// Defines a concrete observer that will be used for testing.
class TestObserver : public TaskManagerObserver {
 public:
  TestObserver(base::TimeDelta refresh_time, int64 resources_flags)
      : TaskManagerObserver(refresh_time, resources_flags) {
  }

  ~TestObserver() override {}

  // task_management::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override {}
  void OnTaskToBeRemoved(TaskId id) override {}
  void OnTasksRefreshed(const TaskIdList& task_ids) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

// Defines a test to validate the behavior of the task manager in response to
// adding and removing different kind of observers.
class TaskManagerObserverTest : public testing::Test {
 public:
  TaskManagerObserverTest() {}
  ~TaskManagerObserverTest() override {}

  TestTaskManager& task_manager() { return task_manager_; }

 private:
  TestTaskManager task_manager_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerObserverTest);
};

}  // namespace

// Validates that the minimum refresh time to be requested is one second. Also
// validates the desired resource flags.
TEST_F(TaskManagerObserverTest, Basic) {
  base::TimeDelta refresh_time1 = base::TimeDelta::FromSeconds(2);
  base::TimeDelta refresh_time2 = base::TimeDelta::FromMilliseconds(999);
  int64 flags1 = RefreshType::REFRESH_TYPE_CPU |
      RefreshType::REFRESH_TYPE_WEBCACHE_STATS |
      RefreshType::REFRESH_TYPE_HANDLES;
  int64 flags2 = RefreshType::REFRESH_TYPE_MEMORY |
      RefreshType::REFRESH_TYPE_NACL;

  TestObserver observer1(refresh_time1, flags1);
  TestObserver observer2(refresh_time2, flags2);

  EXPECT_EQ(refresh_time1, observer1.desired_refresh_time());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1), observer2.desired_refresh_time());
  EXPECT_EQ(flags1, observer1.desired_resources_flags());
  EXPECT_EQ(flags2, observer2.desired_resources_flags());
}

// Validates the behavior of the task manager in response to adding and
// removing observers.
TEST_F(TaskManagerObserverTest, TaskManagerResponseToObservers) {
  EXPECT_EQ(base::TimeDelta::Max(), task_manager().GetRefreshTime());
  EXPECT_EQ(0, task_manager().GetEnabledFlags());

  // Add a bunch of observers and make sure the task manager responds correctly.
  base::TimeDelta refresh_time1 = base::TimeDelta::FromSeconds(3);
  base::TimeDelta refresh_time2 = base::TimeDelta::FromSeconds(10);
  base::TimeDelta refresh_time3 = base::TimeDelta::FromSeconds(3);
  base::TimeDelta refresh_time4 = base::TimeDelta::FromSeconds(2);
  int64 flags1 = RefreshType::REFRESH_TYPE_CPU |
      RefreshType::REFRESH_TYPE_WEBCACHE_STATS |
      RefreshType::REFRESH_TYPE_HANDLES;
  int64 flags2 = RefreshType::REFRESH_TYPE_MEMORY |
      RefreshType::REFRESH_TYPE_NACL;
  int64 flags3 = RefreshType::REFRESH_TYPE_MEMORY |
      RefreshType::REFRESH_TYPE_CPU;
  int64 flags4 = RefreshType::REFRESH_TYPE_GPU_MEMORY;

  TestObserver observer1(refresh_time1, flags1);
  TestObserver observer2(refresh_time2, flags2);
  TestObserver observer3(refresh_time3, flags3);
  TestObserver observer4(refresh_time4, flags4);

  task_manager().AddObserver(&observer1);
  task_manager().AddObserver(&observer2);
  task_manager().AddObserver(&observer3);
  task_manager().AddObserver(&observer4);

  EXPECT_EQ(refresh_time4, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2 | flags3 | flags4,
            task_manager().GetEnabledFlags());

  // Removing observers should also reflect on the refresh time and resource
  // flags.
  task_manager().RemoveObserver(&observer4);
  EXPECT_EQ(refresh_time3, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2 | flags3, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer3);
  EXPECT_EQ(refresh_time1, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1 | flags2, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer2);
  EXPECT_EQ(refresh_time1, task_manager().GetRefreshTime());
  EXPECT_EQ(flags1, task_manager().GetEnabledFlags());
  task_manager().RemoveObserver(&observer1);
  EXPECT_EQ(base::TimeDelta::Max(), task_manager().GetRefreshTime());
  EXPECT_EQ(0, task_manager().GetEnabledFlags());
}

}  // namespace task_management
