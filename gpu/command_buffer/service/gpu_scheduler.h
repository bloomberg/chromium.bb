// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_

#include <queue>

#include "base/atomicops.h"
#include "base/atomic_ref_count.h"
#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/gpu_export.h"

namespace gfx {
class GLFence;
}

namespace gpu {

struct RefCountedCounter
    : public base::RefCountedThreadSafe<RefCountedCounter> {
  base::AtomicRefCount count;
  RefCountedCounter() : count(0) {}

  bool IsZero() { return base::AtomicRefCountIsZero(&count); }
  void IncCount() { base::AtomicRefCountInc(&count); }
  void DecCount() { base::AtomicRefCountDec(&count); }
  void Reset() { base::subtle::NoBarrier_Store(&count, 0); }
 private:
  ~RefCountedCounter() {}

  friend class base::RefCountedThreadSafe<RefCountedCounter>;
};

// This class schedules commands that have been flushed. They are received via
// a command buffer and forwarded to a command parser. TODO(apatrick): This
// class should not know about the decoder. Do not add additional dependencies
// on it.
class GPU_EXPORT GpuScheduler
    : NON_EXPORTED_BASE(public CommandBufferEngine),
      public base::SupportsWeakPtr<GpuScheduler> {
 public:
  GpuScheduler(CommandBuffer* command_buffer,
               AsyncAPIInterface* handler,
               gles2::GLES2Decoder* decoder);

  virtual ~GpuScheduler();

  void PutChanged();

  void SetPreemptByCounter(scoped_refptr<RefCountedCounter> counter) {
    preempt_by_counter_ = counter;
  }

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules. Whether or not the
  // scheduler is currently scheduled is "reference counted". Every call with
  // false must eventually be paired by a call with true.
  void SetScheduled(bool is_scheduled);

  // Returns whether the scheduler is currently able to process more commands.
  bool IsScheduled();

  // Returns whether the scheduler needs to be polled again in the future.
  bool HasMoreWork();

  // Sets a callback that is invoked just before scheduler is rescheduled.
  // Takes ownership of callback object.
  void SetScheduledCallback(const base::Closure& scheduled_callback);

  // Implementation of CommandBufferEngine.
  virtual Buffer GetSharedMemoryBuffer(int32 shm_id) OVERRIDE;
  virtual void set_token(int32 token) OVERRIDE;
  virtual bool SetGetBuffer(int32 transfer_buffer_id) OVERRIDE;
  virtual bool SetGetOffset(int32 offset) OVERRIDE;
  virtual int32 GetGetOffset() OVERRIDE;

  void SetCommandProcessedCallback(const base::Closure& callback);

  void DeferToFence(base::Closure task);

  // Polls the fences, invoking callbacks that were waiting to be triggered
  // by them and returns whether all fences were complete.
  bool PollUnscheduleFences();

  CommandParser* parser() const {
    return parser_.get();
  }

 private:
  // Artificially reschedule if the scheduler is still unscheduled after a
  // timeout.
  void RescheduleTimeOut();

  // The GpuScheduler holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GpuScheduler and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

  // The parser uses this to execute commands.
  AsyncAPIInterface* handler_;

  // Does not own decoder. TODO(apatrick): The GpuScheduler shouldn't need a
  // pointer to the decoder, it is only used to initialize the CommandParser,
  // which could be an argument to the constructor, and to determine the
  // reason for context lost.
  gles2::GLES2Decoder* decoder_;

  // TODO(apatrick): The GpuScheduler currently creates and owns the parser.
  // This should be an argument to the constructor.
  scoped_ptr<CommandParser> parser_;

  // Greater than zero if this is waiting to be rescheduled before continuing.
  int unscheduled_count_;

  // The number of times this scheduler has been artificially rescheduled on
  // account of a timeout.
  int rescheduled_count_;

  // A factory for outstanding rescheduling tasks that is invalidated whenever
  // the scheduler is rescheduled.
  base::WeakPtrFactory<GpuScheduler> reschedule_task_factory_;

  // The GpuScheduler will unschedule itself in the event that further GL calls
  // are issued to it before all these fences have been crossed by the GPU.
  struct UnscheduleFence {
    UnscheduleFence(gfx::GLFence* fence, base::Closure task);
    ~UnscheduleFence();

    scoped_ptr<gfx::GLFence> fence;
    base::Closure task;
  };
  std::queue<linked_ptr<UnscheduleFence> > unschedule_fences_;

  base::Closure scheduled_callback_;
  base::Closure command_processed_callback_;

  // If non-NULL and preempt_by_counter_->count is non-zero,
  // exit PutChanged early.
  scoped_refptr<RefCountedCounter> preempt_by_counter_;
  bool was_preempted_;

  DISALLOW_COPY_AND_ASSIGN(GpuScheduler);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
