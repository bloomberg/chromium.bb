// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_

#include <deque>
#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/abort_callback.h"
#include "storage/browser/fileapi/async_file_util.h"

namespace chromeos {
namespace file_system_provider {

// Queues arbitrary tasks. At most |max_in_parallel_| tasks will be running at
// once.
//
// The common use case is:
// 1. Call NewToken() to obtain the token used bo all other methods.
// 2. Call Enqueue() to enqueue the task.
// 3. Call Complete() when the task is completed.
// 4. Call Remove() to remove a completed task from the queue and run other
//    enqueued tasks.
//
// Enqueued tasks can be aborted with Abort() at any time until they are marked
// as completed or removed from the queue, as long as the task supports aborting
// (it's abort callback is not NULL). Aorting does not remove the task from the
// queue.
//
// In most cases you'll want to call Remove() and Complete() one after the
// other. However, in some cases you may want to separate it. Eg. for limiting
// number of opened files, you may want to call Complete() after opening is
// completed, but Remove() after the file is closed. Note, that they can be
// called at most once.
class Queue {
 public:
  typedef base::Callback<AbortCallback(void)> AbortableCallback;

  // Creates a queue with a maximum number of tasks running in parallel.
  explicit Queue(size_t max_in_parallel);

  virtual ~Queue();

  // Creates a token for enqueuing (and later aborting) tasks.
  size_t NewToken();

  // Enqueues a task using a token generated with NewToken(). The task will be
  // executed if there is space in the internal queue, otherwise it will wait
  // until another task is finished. Once the task is finished, Complete() and
  // Remove() must be called. The callback's abort callback may be NULL. In
  // such case, Abort() must not be called.
  void Enqueue(size_t token, const AbortableCallback& callback);

  // Forcibly aborts a previously enqueued task. May be called at any time as
  // long as the task is still in the queue and is not marked as completed.
  // Note, that Remove() must be called in order to remove the task from the
  // queue. Must not be called if the task doesn't support aborting (it's
  // abort callback is NULL).
  void Abort(size_t token);

  // Returns true if the task which is in the queue with |token| has been
  // aborted. This method must not be called for tasks which are not in the
  // queue.
  bool IsAborted(size_t token);

  // Marks the previously enqueued task as complete. Must be called for each
  // enqueued task (unless aborted). Note, that Remove() must be called in order
  // to remove the task from the queue if it hasn't been aborted earlier.
  // It must not be called more than one, nor for aborted tasks.
  void Complete(size_t token);

  // Removes the previously enqueued and completed or aborted task from the
  // queue. Must not be called more than once.
  void Remove(size_t token);

 private:
  // Information about an enqueued task which hasn't been removed, nor aborted.
  struct Task {
    Task();
    Task(size_t token, const AbortableCallback& callback);
    ~Task();

    size_t token;
    AbortableCallback callback;
    AbortCallback abort_callback;
  };

  // Runs the next task from the pending queue if there is less than
  // |max_in_parallel_| tasks running at once.
  void MaybeRun();

  const size_t max_in_parallel_;
  size_t next_token_;
  std::deque<Task> pending_;
  std::map<int, Task> executed_;
  std::map<int, Task> completed_;
  std::map<int, Task> aborted_;

  base::WeakPtrFactory<Queue> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(Queue);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_QUEUE_H_
