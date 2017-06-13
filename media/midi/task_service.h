// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_TASK_SERVICE_H_
#define MEDIA_MIDI_TASK_SERVICE_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/midi/midi_export.h"

namespace midi {

// TaskService manages TaskRunners that can be used in midi and provides
// functionalities to ensure thread safety.
class MIDI_EXPORT TaskService final {
 public:
  using RunnerId = size_t;
  using InstanceId = int;

  static constexpr RunnerId kDefaultRunnerId = 0;

  TaskService();
  ~TaskService();

  // Issues an InstanceId internally to post tasks via PostBoundTask() and
  // PostDelayedBoundTask() with the InstanceId. Once UnbindInstance() is
  // called, tasks posted via these methods with unbind InstanceId won't be
  // invoked any more.
  // Returns true if call is bound or unbound correctly. Otherwise returns
  // false, that happens when the BindInstance() is called twice without
  // unbinding the previous instance.
  bool BindInstance();
  bool UnbindInstance();

  // Posts a task to run on a specified TaskRunner. |runner_id| should be
  // kDefaultRunnerId or a positive number. If kDefaultRunnerId is specified
  // the task runs on the thread on which BindInstance() is called. Other number
  // will run the task on a dedicated thread that is bound to the |runner_id|.
  void PostStaticTask(RunnerId runner_id, base::OnceClosure task);

  // Posts a task to run on a specified TaskRunner, and ensures that the bound
  // instance should not quit UnbindInstance() while a bound task is running.
  // See PostStaticTask() for |runner_id|.
  void PostBoundTask(RunnerId runner, base::OnceClosure task);
  void PostBoundDelayedTask(RunnerId runner_id,
                            base::OnceClosure task,
                            base::TimeDelta delay);

 private:
  // Returns a SingleThreadTaskRunner reference. Each TaskRunner will be
  // constructed on demand.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(RunnerId runner_id);

  // Helps to run a posted bound task on TaskRunner safely.
  void RunTask(InstanceId instance_id,
               RunnerId runner_id,
               base::OnceClosure task);

  // Keeps a TaskRunner for the thread that calls BindInstance() as a default
  // task runner to run posted tasks.
  scoped_refptr<base::SingleThreadTaskRunner> default_task_runner_;

  // Holds threads to host SingleThreadTaskRunners.
  std::vector<std::unique_ptr<base::Thread>> threads_;

  // Holds lock objects to ensure that tasks run while the instance is bound.
  std::vector<std::unique_ptr<base::Lock>> thread_task_locks_;

  // Holds InstanceId for the next bound instance.
  InstanceId next_instance_id_;

  // Holds InstanceId for the current bound instance.
  InstanceId bound_instance_id_;

  // Protects |next_instance_id_| and |bound_instance_id_|.
  base::Lock instance_lock_;

  // Protects all other members.
  base::Lock lock_;

  // If multiple locks should be obtained simultaneously, we should acquire them
  // in the order below so to avoid deadklocks.
  // |instance_lock_| -> |lock_| -> |(one of) thread_task_locks_|.

  DISALLOW_COPY_AND_ASSIGN(TaskService);
};

};  // namespace midi

#endif  // MEDIA_MIDI_TASK_SERVICE_H_
