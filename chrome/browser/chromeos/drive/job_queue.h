// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_JOB_QUEUE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_JOB_QUEUE_H_

#include <deque>
#include <set>
#include <vector>

#include "chrome/browser/chromeos/drive/job_list.h"

namespace drive {

// Priority queue for managing jobs in JobScheduler.
class JobQueue {
 public:
  // Creates a queue that allows |num_max_concurrent_jobs| concurrent job
  // execution and has |num_priority_levels| levels of priority.
  JobQueue(size_t num_max_concurrent_jobs, size_t num_priority_levels);
  ~JobQueue();

  // Pushes a job |id| of |priority|. The job with the smallest priority value
  // is popped first (lower values are higher priority). In the same priority,
  // the queue is "first-in first-out".
  void Push(JobID id, int priority);

  // Pops the first job which meets |accepted_priority| (i.e. the first job in
  // the queue with equal or higher priority (lower value)), and the limit of
  // concurrent job count is satisfied.
  //
  // For instance, if |accepted_priority| is 1, the first job with priority 0
  // (higher priority) in the queue is picked even if a job with priority 1 was
  // pushed earlier. If there is no job with priority 0, the first job with
  // priority 1 in the queue is picked.
  bool PopForRun(int accepted_priority, JobID* id);

  // Gets queued jobs with the given priority.
  void GetQueuedJobs(int priority, std::vector<JobID>* jobs) const;

  // Marks a running job |id| as finished running. This decreases the count
  // of running parallel jobs and makes room for other jobs to be popped.
  void MarkFinished(JobID id);

  // Generates a string representing the internal state for logging.
  std::string ToString() const;

  // Gets the total number of jobs in the queue.
  size_t GetNumberOfJobs() const;

  // Removes the job from the queue.
  void Remove(JobID id);

 private:
  size_t num_max_concurrent_jobs_;
  std::vector<std::deque<JobID> > queue_;
  std::set<JobID> running_;

  DISALLOW_COPY_AND_ASSIGN(JobQueue);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_JOB_QUEUE_H_
