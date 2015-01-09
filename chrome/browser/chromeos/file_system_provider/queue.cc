// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_system_provider/queue.h"

namespace chromeos {
namespace file_system_provider {

struct Queue::Task {
  Task() : token(0), completed(false) {}
  Task(size_t token, const AbortableCallback& callback)
      : token(token), completed(false), callback(callback) {}

  size_t token;
  bool completed;
  AbortableCallback callback;
  AbortCallback abort_callback;
};

Queue::Queue(size_t max_in_parallel)
    : max_in_parallel_(max_in_parallel),
      next_token_(1),
      weak_ptr_factory_(this) {
  DCHECK_LT(0u, max_in_parallel);
}

Queue::~Queue() {
}

size_t Queue::NewToken() {
  return next_token_++;
}

AbortCallback Queue::Enqueue(size_t token, const AbortableCallback& callback) {
#if !NDEBUG
  const auto it = executed_.find(token);
  DCHECK(it == executed_.end());
  for (auto& task : pending_) {
    DCHECK(token != task.token);
  }
#endif
  pending_.push_back(Task(token, callback));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
  return base::Bind(&Queue::Abort, weak_ptr_factory_.GetWeakPtr(), token);
}

void Queue::Complete(size_t token) {
  const auto it = executed_.find(token);
  DCHECK(it != executed_.end() && !it->second.completed);
  it->second.completed = true;
}

void Queue::Remove(size_t token) {
  const auto it = executed_.find(token);
  DCHECK(it != executed_.end() && it->second.completed);

  executed_.erase(it);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
}

void Queue::MaybeRun() {
  if (executed_.size() == max_in_parallel_ || !pending_.size()) {
    return;
  }

  DCHECK_GT(max_in_parallel_, executed_.size());
  Task task = pending_.front();
  pending_.pop_front();

  executed_[task.token] = task;
  executed_[task.token].abort_callback = task.callback.Run();
}

void Queue::Abort(size_t token,
                  const storage::AsyncFileUtil::StatusCallback& callback) {
  // Check if it's running.
  const auto it = executed_.find(token);
  if (it != executed_.end()) {
    const Task& task = it->second;
    // If the task is marked as completed, then it's impossible to abort it.
    if (task.completed) {
      callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
      return;
    }
    DCHECK(!task.abort_callback.is_null());
    it->second.abort_callback.Run(callback);
    executed_.erase(it);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Aborting not running tasks is linear. TODO(mtomasz): Optimize if feasible.
  for (auto it = pending_.begin(); it != pending_.end(); ++it) {
    if (token == it->token) {
      pending_.erase(it);
      callback.Run(base::File::FILE_OK);
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::Bind(&Queue::MaybeRun, weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }

  // The task is already removed.
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

}  // namespace file_system_provider
}  // namespace chromeos
