// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_queue.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

TEST(JobQueueTest, BasicJobQueueOperations) {
  const int kNumMaxConcurrentJobs = 3;
  const int kNumPriorityLevels = 2;
  enum {HIGH_PRIORITY, LOW_PRIORITY};

  // Create a queue. Number of jobs are initially zero.
  JobQueue queue(kNumMaxConcurrentJobs, kNumPriorityLevels);
  EXPECT_EQ(0U, queue.GetNumberOfJobs());

  // Push 4 jobs.
  queue.Push(101, LOW_PRIORITY);
  queue.Push(102, HIGH_PRIORITY);
  queue.Push(103, LOW_PRIORITY);
  queue.Push(104, HIGH_PRIORITY);

  // High priority jobs should be popped first.
  JobID id;
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(102, id);
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(104, id);

  // Then low priority jobs follow.
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(101, id);

  // The queue allows at most 3 parallel runs. So returns false here.
  EXPECT_FALSE(queue.PopForRun(LOW_PRIORITY, &id));

  // No jobs finished yet, so the job count is four.
  EXPECT_EQ(4U, queue.GetNumberOfJobs());

  // Mark one job as finished.
  queue.MarkFinished(104);
  EXPECT_EQ(3U, queue.GetNumberOfJobs());

  // Then the next jobs can be popped.
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(103, id);

  // Push 1 more job.
  queue.Push(105, LOW_PRIORITY);

  // Finish all 3 running jobs.
  queue.MarkFinished(101);
  queue.MarkFinished(102);
  queue.MarkFinished(103);
  EXPECT_EQ(1U, queue.GetNumberOfJobs());

  // The remaining jobs is of low priority, so under HIGH_PRIORITY context, it
  // cannot be popped for running.
  EXPECT_FALSE(queue.PopForRun(HIGH_PRIORITY, &id));

  // Under the low priority context, it is fine.
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(105, id);
}

TEST(JobQueueTest, JobQueueRemove) {
  const int kNumMaxConcurrentJobs = 3;
  const int kNumPriorityLevels = 2;
  enum {HIGH_PRIORITY, LOW_PRIORITY};

  // Create a queue. Number of jobs are initially zero.
  JobQueue queue(kNumMaxConcurrentJobs, kNumPriorityLevels);
  EXPECT_EQ(0U, queue.GetNumberOfJobs());

  // Push 4 jobs.
  queue.Push(101, LOW_PRIORITY);
  queue.Push(102, HIGH_PRIORITY);
  queue.Push(103, LOW_PRIORITY);
  queue.Push(104, HIGH_PRIORITY);
  EXPECT_EQ(4U, queue.GetNumberOfJobs());

  // Remove 2.
  queue.Remove(101);
  queue.Remove(104);
  EXPECT_EQ(2U, queue.GetNumberOfJobs());

  // Pop the 2 jobs.
  JobID id;
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(102, id);
  EXPECT_TRUE(queue.PopForRun(LOW_PRIORITY, &id));
  EXPECT_EQ(103, id);
  queue.MarkFinished(102);
  queue.MarkFinished(103);

  // 0 job left.
  EXPECT_EQ(0U, queue.GetNumberOfJobs());
  EXPECT_FALSE(queue.PopForRun(LOW_PRIORITY, &id));
}

}  // namespace drive
