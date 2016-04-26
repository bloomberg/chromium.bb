// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/scheduler_unique_stack.h"

#include "base/task_scheduler/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

// Verify that Push() and Pop() add/remove values in FIFO order.
TEST(TaskSchedulerUniqueStackTest, PushPop) {
  SchedulerUniqueStack<int> stack;
  EXPECT_TRUE(stack.Empty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(1);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(2);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(3);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(3, stack.Pop());
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(3);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(3U, stack.Size());

  EXPECT_EQ(3, stack.Pop());
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(2, stack.Pop());
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(1, stack.Pop());
  EXPECT_TRUE(stack.Empty());
  EXPECT_EQ(0U, stack.Size());
}

// Verify that a value can be removed by Remove().
TEST(TaskSchedulerUniqueStackTest, Remove) {
  SchedulerUniqueStack<int> stack;
  EXPECT_TRUE(stack.Empty());
  EXPECT_EQ(0U, stack.Size());

  stack.Push(1);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(1U, stack.Size());

  stack.Push(2);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(2U, stack.Size());

  stack.Push(3);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(3U, stack.Size());

  stack.Remove(2);
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(2U, stack.Size());

  EXPECT_EQ(3, stack.Pop());
  EXPECT_FALSE(stack.Empty());
  EXPECT_EQ(1U, stack.Size());

  EXPECT_EQ(1, stack.Pop());
  EXPECT_TRUE(stack.Empty());
  EXPECT_EQ(0U, stack.Size());
}

// Verify that a value can be pushed again after it has been removed.
TEST(TaskSchedulerUniqueStackTest, PushAfterRemove) {
  SchedulerUniqueStack<int> stack;
  EXPECT_EQ(0U, stack.Size());
  EXPECT_TRUE(stack.Empty());

  stack.Push(5);
  EXPECT_EQ(1U, stack.Size());
  EXPECT_FALSE(stack.Empty());

  stack.Remove(5);
  EXPECT_EQ(0U, stack.Size());
  EXPECT_TRUE(stack.Empty());

  stack.Push(5);
  EXPECT_EQ(1U, stack.Size());
  EXPECT_FALSE(stack.Empty());
}

// Verify that Push() DCHECKs when a value is inserted twice.
TEST(TaskSchedulerUniqueStackTest, PushTwice) {
  SchedulerUniqueStack<int> stack;
  stack.Push(5);
  EXPECT_DCHECK_DEATH({ stack.Push(5); }, "");
}

}  // namespace internal
}  // namespace base
