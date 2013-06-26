// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/job_queue.h"

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace drive {

JobQueue::JobQueue(size_t num_max_concurrent_jobs,
                   size_t num_priority_levels)
    : num_max_concurrent_jobs_(num_max_concurrent_jobs),
      queue_(num_priority_levels) {
}

JobQueue::~JobQueue() {
}

bool JobQueue::PopForRun(int accepted_priority, JobID* id) {
  DCHECK_LT(accepted_priority, static_cast<int>(queue_.size()));

  // Too many jobs are running already.
  if (running_.size() >= num_max_concurrent_jobs_)
    return false;

  // Looks up the queue in the order of priority upto |accepted_priority|.
  for (int priority = 0; priority <= accepted_priority; ++priority) {
    if (!queue_[priority].empty()) {
      *id = queue_[priority].front();
      queue_[priority].pop_front();
      running_.insert(*id);
      return true;
    }
  }
  return false;
}

void JobQueue::GetQueuedJobs(int priority, std::vector<JobID>* jobs) const {
  DCHECK_LT(priority, static_cast<int>(queue_.size()));

  jobs->assign(queue_[priority].begin(), queue_[priority].end());
}

void JobQueue::Push(JobID id, int priority) {
  DCHECK_LT(priority, static_cast<int>(queue_.size()));

  queue_[priority].push_back(id);
}

void JobQueue::MarkFinished(JobID id) {
  size_t num_erased = running_.erase(id);
  DCHECK_EQ(1U, num_erased);
}

std::string JobQueue::ToString() const {
  size_t pending = 0;
  for (size_t i = 0; i < queue_.size(); ++i)
    pending += queue_[i].size();
  return base::StringPrintf("pending: %d, running: %d",
                            static_cast<int>(pending),
                            static_cast<int>(running_.size()));
}

size_t JobQueue::GetNumberOfJobs() const {
  size_t count = running_.size();
  for (size_t i = 0; i < queue_.size(); ++i)
    count += queue_[i].size();
  return count;
}

void JobQueue::Remove(JobID id) {
  for (size_t i = 0; i < queue_.size(); ++i) {
    std::deque<JobID>::iterator iter =
        std::find(queue_[i].begin(), queue_[i].end(), id);
    if (iter != queue_[i].end()) {
      queue_[i].erase(iter);
      break;
    }
  }
}

}  // namespace drive
