// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/chromeos/resource_reporter/resource_reporter.h"
#include "chrome/browser/task_management/test_task_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using task_management::TaskId;

namespace chromeos {

namespace {

const int64_t k1KB = 1024;
const int64_t k1MB = 1024 * 1024;
const int64_t k1GB = 1024 * 1024 * 1024;

// A list of task records that we'll use to fill the task manager.
const ResourceReporter::TaskRecord kTestTasks[] = {
    { 0, "0", 30.0, 43 * k1KB, false },
    { 1, "1", 9.0, 20 * k1MB, false },
    { 2, "2", 35.0, 3 * k1GB, false },
    { 3, "3", 21.0, 300 * k1MB, false },  // Browser task.
    { 4, "4", 85.0, 400 * k1KB, false },
    { 5, "5", 30.1, 500 * k1MB, false },
    { 6, "6", 60.0, 900 * k1MB, false },  // GPU task.
    { 7, "7", 4.0, 1 * k1GB, false },
    { 8, "8", 40.0, 64 * k1KB, false },
    { 9, "9", 93.0, 64 * k1MB, false },
    { 10, "10", 2.23, 2 * k1KB, false },
    { 11, "11", 55.0, 40 * k1MB, false },
    { 12, "12", 87.0, 30 * k1KB, false },
};

// A list of task IDs that will be removed from the task manager later after all
// the above tasks have been added.
const task_management::TaskId kIdsOfTasksToRemove[] = {
    4, 7, 12, 8, 5,
};

// Must be larger than |ResourceReporter::kTopConsumerCount| + 2 (to account for
// the browser and GPU tasks).
const size_t kTasksSize = arraysize(kTestTasks);

// Must be smaller than |ResourceReporter::kTopConsumerCount|.
const size_t kInitiallyAddedTasks = kTasksSize - 6;

// Must be such that |kTasksSize| - |kTasksToBeRemovedSize| is less than
// |ResourceReporter::kTopConsumerCount|.
const size_t kTasksToBeRemovedSize = arraysize(kIdsOfTasksToRemove);

// A test implementation of the task manager that can be used to collect CPU and
// memory usage so that they can be tested with the resource reporter.
class DummyTaskManager : public task_management::TestTaskManager {
 public:
  DummyTaskManager() {
    set_timer_for_testing(
        std::unique_ptr<base::Timer>(new base::MockTimer(false, false)));
  }
  ~DummyTaskManager() override {}

  // task_management::TestTaskManager:
  double GetCpuUsage(TaskId task_id) const override {
    return tasks_.at(task_id)->cpu_percent;
  }
  int64_t GetPhysicalMemoryUsage(TaskId task_id) const override {
    return tasks_.at(task_id)->memory_bytes;
  }
  const std::string& GetTaskNameForRappor(TaskId task_id) const override {
    return tasks_.at(task_id)->task_name_for_rappor;
  }
  task_management::Task::Type GetType(TaskId task_id) const override {
    switch (task_id) {
      case 3:
        return task_management::Task::BROWSER;

      case 6:
        return task_management::Task::GPU;

      default:
        return task_management::Task::RENDERER;
    }
  }

  void AddTaskFromIndex(size_t index) {
    tasks_[kTestTasks[index].id] = &kTestTasks[index];
  }

  void RemoveTask(TaskId id) {
    NotifyObserversOnTaskToBeRemoved(id);

    tasks_.erase(id);
  }

  void ManualRefresh() {
    ids_.clear();
    for (const auto& pair : tasks_) {
      ids_.push_back(pair.first);
    }

    NotifyObserversOnRefresh(ids_);
  }

 private:
  std::map<TaskId, const ResourceReporter::TaskRecord*> tasks_;

  DISALLOW_COPY_AND_ASSIGN(DummyTaskManager);
};

}  // namespace

class ResourceReporterTest : public testing::Test {
 public:
  ResourceReporterTest() {}
  ~ResourceReporterTest() override {}

  // Start / Stop observing the task manager.
  void Start() {
    task_manager_.AddObserver(resource_reporter());
  }
  void Stop() {
    task_manager_.RemoveObserver(resource_reporter());
  }

  // Adds a number of tasks less than |kTopConsumersCount| to the task manager.
  void AddInitialTasks() {
    for (size_t i = 0; i < kInitiallyAddedTasks; ++i)
      task_manager_.AddTaskFromIndex(i);
  }

  // Adds all the remaining tasks to the task manager so that we have more than
  // |kTopConsumersCount|.
  void AddRemainingTasks() {
    for (size_t i = kInitiallyAddedTasks; i < kTasksSize; ++i)
      task_manager_.AddTaskFromIndex(i);
  }

  // Remove the task with |id| from the task manager.
  void RemoveTask(TaskId id) {
    task_manager_.RemoveTask(id);
  }

  // Manually refresh the task manager.
  void RefreshTaskManager() {
    task_manager_.ManualRefresh();
  }

  // Tests that the task records in |ResourceReporter::task_records_by_cpu_| are
  // properly sorted by the CPU usage in a descending order.
  bool IsCpuRecordsSetSorted() const {
    double current_cpu = std::numeric_limits<double>::max();
    for (const auto& record : resource_reporter()->task_records_by_cpu_) {
      if (record->cpu_percent > current_cpu)
        return false;
      current_cpu = record->cpu_percent;
    }

    return true;
  }

  // Tests that the task records in |ResourceReporter::task_records_by_memory_|
  // are properly sorted by the memory usage in a descending order.
  bool IsMemoryRecordsSetSorted() const {
    int64_t current_memory = std::numeric_limits<int64_t>::max();
    for (const auto& record : resource_reporter()->task_records_by_memory_) {
      if (record->memory_bytes > current_memory)
        return false;
      current_memory = record->memory_bytes;
    }

    return true;
  }

  ResourceReporter* resource_reporter() const {
    return ResourceReporter::GetInstance();
  }

 private:
  DummyTaskManager task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ResourceReporterTest);
};

// Tests that ResourceReporter::GetCpuRapporMetricName() returns the correct
// metric name that corresponds to the given CPU usage.
TEST_F(ResourceReporterTest, TestGetCpuRapporMetricName) {
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(0.3));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(5.7));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(9.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_0_TO_10_PERCENT,
            ResourceReporter::GetCpuUsageRange(10.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(10.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(29.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_10_TO_30_PERCENT,
            ResourceReporter::GetCpuUsageRange(30.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(30.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(59.99));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_30_TO_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(60.0));

  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_ABOVE_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(60.1));
  EXPECT_EQ(ResourceReporter::CpuUsageRange::RANGE_ABOVE_60_PERCENT,
            ResourceReporter::GetCpuUsageRange(100.0));
}

// Tests that ResourceReporter::GetMemoryRapporMetricName() returns the correct
// metric names for the given memory usage.
TEST_F(ResourceReporterTest, TestGetMemoryRapporMetricName) {
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(2 * k1KB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(20 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_0_TO_200_MB,
            ResourceReporter::GetMemoryUsageRange(200 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_200_TO_400_MB,
            ResourceReporter::GetMemoryUsageRange(201 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_200_TO_400_MB,
            ResourceReporter::GetMemoryUsageRange(400 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_400_TO_600_MB,
            ResourceReporter::GetMemoryUsageRange(401 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_400_TO_600_MB,
            ResourceReporter::GetMemoryUsageRange(600 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_600_TO_800_MB,
            ResourceReporter::GetMemoryUsageRange(601 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_600_TO_800_MB,
            ResourceReporter::GetMemoryUsageRange(800 * k1MB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_800_TO_1_GB,
            ResourceReporter::GetMemoryUsageRange(801 * k1MB));
  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_800_TO_1_GB,
            ResourceReporter::GetMemoryUsageRange(1 * k1GB));

  EXPECT_EQ(ResourceReporter::MemoryUsageRange::RANGE_ABOVE_1_GB,
            ResourceReporter::GetMemoryUsageRange(1 * k1GB + 1 * k1KB));
}

// Tests all the interactions between the resource reporter and the task
// manager.
TEST_F(ResourceReporterTest, TestAll) {
  // First start by making sure that our assumptions are correct.
  ASSERT_LT(kInitiallyAddedTasks, ResourceReporter::kTopConsumersCount);
  ASSERT_LT(ResourceReporter::kTopConsumersCount, kTasksSize - 2);
  ASSERT_LT(kTasksSize - kTasksToBeRemovedSize - 2,
            ResourceReporter::kTopConsumersCount);

  Start();

  // Add the initial tasks to the task manager, and expect that after a refresh
  // that the resource reporter is already tracking them, and the records are
  // sorted properly.
  AddInitialTasks();
  RefreshTaskManager();
  // Browser and GPU tasks won't be added.
  EXPECT_EQ(kInitiallyAddedTasks - 2,
            resource_reporter()->task_records_.size());
  EXPECT_LE(resource_reporter()->task_records_by_cpu_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_LE(resource_reporter()->task_records_by_memory_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_TRUE(IsCpuRecordsSetSorted());
  EXPECT_TRUE(IsMemoryRecordsSetSorted());

  // After adding the remaining tasks which are more than |kTopConsumerCount|,
  // we must expect that the records used for recording the samples are EQUAL to
  // |kTopConsumerCount|, and the records are still properly sorted.
  AddRemainingTasks();
  RefreshTaskManager();
  EXPECT_EQ(kTasksSize - 2, resource_reporter()->task_records_.size());
  EXPECT_EQ(ResourceReporter::kTopConsumersCount,
            resource_reporter()->task_records_by_cpu_.size());
  EXPECT_EQ(ResourceReporter::kTopConsumersCount,
            resource_reporter()->task_records_by_memory_.size());
  EXPECT_TRUE(IsCpuRecordsSetSorted());
  EXPECT_TRUE(IsMemoryRecordsSetSorted());

  // Removing tasks from the task manager must result in their removal from the
  // resource reporter immediately without having to wait until the next
  // refresh.
  for (const auto& id : kIdsOfTasksToRemove)
    RemoveTask(id);
  const size_t kExpectedTasksCount = kTasksSize - kTasksToBeRemovedSize - 2;
  EXPECT_EQ(kExpectedTasksCount, resource_reporter()->task_records_.size());
  EXPECT_LE(resource_reporter()->task_records_by_cpu_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_LE(resource_reporter()->task_records_by_memory_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_TRUE(IsCpuRecordsSetSorted());
  EXPECT_TRUE(IsMemoryRecordsSetSorted());

  // After refresh nothing changes.
  RefreshTaskManager();
  EXPECT_EQ(kExpectedTasksCount, resource_reporter()->task_records_.size());
  EXPECT_LE(resource_reporter()->task_records_by_cpu_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_LE(resource_reporter()->task_records_by_memory_.size(),
            ResourceReporter::kTopConsumersCount);
  EXPECT_TRUE(IsCpuRecordsSetSorted());
  EXPECT_TRUE(IsMemoryRecordsSetSorted());

  Stop();
}

}  // namespace chromeos
