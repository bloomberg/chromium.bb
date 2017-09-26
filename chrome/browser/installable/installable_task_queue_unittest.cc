// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/installable/installable_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

using IconPurpose = content::Manifest::Icon::IconPurpose;

class InstallableTaskQueueUnitTest : public testing::Test {};

// Constructs an InstallableTask, with the supplied bools stored in it.
InstallableTask CreateTask(bool check_installable,
                           bool fetch_valid_primary_icon,
                           bool fetch_valid_badge_icon) {
  InstallableTask task;
  task.first.check_installable = check_installable;
  task.first.fetch_valid_primary_icon = fetch_valid_primary_icon;
  task.first.fetch_valid_badge_icon = fetch_valid_badge_icon;
  return task;
}

bool IsEqual(const InstallableTask& task1, const InstallableTask& task2) {
  return task1.first.check_installable == task2.first.check_installable &&
         task1.first.fetch_valid_primary_icon ==
             task2.first.fetch_valid_primary_icon &&
         task1.first.fetch_valid_badge_icon ==
             task2.first.fetch_valid_badge_icon;
}

TEST_F(InstallableTaskQueueUnitTest, PausingMakesNextTaskAvailable) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = CreateTask(false, false, false);
  InstallableTask task2 = CreateTask(true, true, true);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  // There is another task in the main queue, so it becomes current.
  task_queue.PauseCurrent();
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
}

TEST_F(InstallableTaskQueueUnitTest, PausedTaskCanBeRetrieved) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = CreateTask(false, false, false);
  InstallableTask task2 = CreateTask(true, true, true);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  task_queue.PauseCurrent();
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  task_queue.UnpauseAll();
  // We've unpaused "1", but "2" is still current.
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  task_queue.Next();
  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
}

TEST_F(InstallableTaskQueueUnitTest, NextDiscardsTask) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = CreateTask(false, false, false);
  InstallableTask task2 = CreateTask(true, true, true);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_TRUE(IsEqual(task1, task_queue.Current()));
  task_queue.Next();
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  // Next() does not pause "1"; it just drops it, so there is nothing to
  // unpause.
  task_queue.UnpauseAll();
  // "2" is still current.
  EXPECT_TRUE(IsEqual(task2, task_queue.Current()));
  // Unpausing does not retrieve "1"; it's gone forever.
  task_queue.Next();
  EXPECT_FALSE(task_queue.HasCurrent());
}
