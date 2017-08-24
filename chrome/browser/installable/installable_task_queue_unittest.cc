// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/installable/installable_manager.h"

#include "testing/gtest/include/gtest/gtest.h"

class InstallableTaskQueueUnitTest : public testing::Test {};

// Constructs an InstallableTask, with the supplied tag stored in it.  The tag
// may be retrieved by passing the task to Tag().
InstallableTask TaggedTask(int tag) {
  InstallableTask task;
  task.first.ideal_primary_icon_size_in_px = tag;
  return task;
}

int Tag(InstallableTask task) {
  return task.first.ideal_primary_icon_size_in_px;
}

TEST_F(InstallableTaskQueueUnitTest, PausingMakesNextTaskAvailable) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = TaggedTask(1);
  InstallableTask task2 = TaggedTask(2);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_EQ(1, Tag(task_queue.Current()));
  // There is another task in the main queue, so it becomes current.
  task_queue.PauseCurrent();
  EXPECT_EQ(2, Tag(task_queue.Current()));
}

TEST_F(InstallableTaskQueueUnitTest, PausedTaskCanBeRetrieved) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = TaggedTask(1);
  InstallableTask task2 = TaggedTask(2);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_EQ(1, Tag(task_queue.Current()));
  task_queue.PauseCurrent();
  EXPECT_EQ(2, Tag(task_queue.Current()));
  task_queue.UnpauseAll();
  // We've unpaused "1", but "2" is still current.
  EXPECT_EQ(2, Tag(task_queue.Current()));
  task_queue.Next();
  EXPECT_EQ(1, Tag(task_queue.Current()));
}

TEST_F(InstallableTaskQueueUnitTest, NextDiscardsTask) {
  InstallableTaskQueue task_queue;
  InstallableTask task1 = TaggedTask(1);
  InstallableTask task2 = TaggedTask(2);

  task_queue.Add(task1);
  task_queue.Add(task2);

  EXPECT_EQ(1, Tag(task_queue.Current()));
  task_queue.Next();
  EXPECT_EQ(2, Tag(task_queue.Current()));
  // Next() does not pause "1"; it just drops it, so there is nothing to
  // unpause.
  task_queue.UnpauseAll();
  // "2" is still current.
  EXPECT_EQ(2, Tag(task_queue.Current()));
  // Unpausing does not retrieve "1"; it's gone forever.
  task_queue.Next();
  EXPECT_FALSE(task_queue.HasCurrent());
}
