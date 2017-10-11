// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "gpu/gpu_export.h"

namespace base {
class SingleThreadTaskRunner;
namespace trace_event {
class ConvertableToTraceFormat;
}
}

namespace gpu {
class SyncPointManager;

class GPU_EXPORT Scheduler {
 public:
  struct GPU_EXPORT Task {
    Task(SequenceId sequence_id,
         base::OnceClosure closure,
         std::vector<SyncToken> sync_token_fences);
    Task(Task&& other);
    ~Task();
    Task& operator=(Task&& other);

    SequenceId sequence_id;
    base::OnceClosure closure;
    std::vector<SyncToken> sync_token_fences;
  };

  Scheduler(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
            SyncPointManager* sync_point_manager);

  virtual ~Scheduler();

  // Create a sequence with given priority. Returns an identifier for the
  // sequence that can be used with SyncPonintManager for creating sync point
  // release clients. Sequences start off as enabled (see |EnableSequence|).
  SequenceId CreateSequence(SchedulingPriority priority);

  // Destroy the sequence and run any scheduled tasks immediately.
  void DestroySequence(SequenceId sequence_id);

  // Enables the sequence so that its tasks may be scheduled.
  void EnableSequence(SequenceId sequence_id);

  // Disables the sequence.
  void DisableSequence(SequenceId sequence_id);

  // Raise priority of sequence for client wait (WaitForGetOffset/TokenInRange)
  // on given command buffer.
  void RaisePriorityForClientWait(SequenceId sequence_id,
                                  CommandBufferId command_buffer_id);

  // Reset priority of sequence if it was increased for a client wait.
  void ResetPriorityForClientWait(SequenceId sequence_id,
                                  CommandBufferId command_buffer_id);

  // Schedules task (closure) to run on the sequence. The task is blocked until
  // the sync token fences are released or determined to be invalid. Tasks are
  // run in the order in which they are submitted.
  void ScheduleTask(Task task);

  void ScheduleTasks(std::vector<Task> tasks);

  // Continue running task on the sequence with the closure. This must be called
  // while running a previously scheduled task.
  void ContinueTask(SequenceId sequence_id, base::OnceClosure closure);

  // If the sequence should yield so that a higher priority sequence may run.
  bool ShouldYield(SequenceId sequence_id);

 private:
  class Sequence;

  struct SchedulingState {
    static bool Comparator(const SchedulingState& lhs,
                           const SchedulingState& rhs) {
      return rhs.RunsBefore(lhs);
    }

    SchedulingState();
    SchedulingState(const SchedulingState& other);
    ~SchedulingState();

    bool RunsBefore(const SchedulingState& other) const {
      return std::tie(priority, order_num) <
             std::tie(other.priority, other.order_num);
    }

    std::unique_ptr<base::trace_event::ConvertableToTraceFormat> AsValue()
        const;

    SequenceId sequence_id;
    SchedulingPriority priority = SchedulingPriority::kLow;
    uint32_t order_num = 0;
  };

  void SyncTokenFenceReleased(const SyncToken& sync_token,
                              uint32_t order_num,
                              SequenceId release_sequence_id,
                              SequenceId waiting_sequence_id);

  void ScheduleTaskHelper(Task task);

  void TryScheduleSequence(Sequence* sequence);

  void RebuildSchedulingQueue();

  Sequence* GetSequence(SequenceId sequence_id);

  void RunNextTask();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  SyncPointManager* const sync_point_manager_;

  mutable base::Lock lock_;

  // The following are protected by |lock_|.
  bool running_ = false;

  base::flat_map<SequenceId, std::unique_ptr<Sequence>> sequences_;

  // Used as a priority queue for scheduling sequences. Min heap of
  // SchedulingState with highest priority (lowest order) in front.
  std::vector<SchedulingState> scheduling_queue_;

  // If the scheduling queue needs to be rebuild because a sequence changed
  // priority.
  bool rebuild_scheduling_queue_ = false;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<Scheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SCHEDULER_H_
