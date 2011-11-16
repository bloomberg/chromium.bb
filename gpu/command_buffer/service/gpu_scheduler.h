// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_

#include <queue>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {

// This class schedules commands that have been flushed. They are received via
// a command buffer and forwarded to a command parser. TODO(apatrick): This
// class should not know about the decoder. Do not add additional dependencies
// on it.
class GpuScheduler
    : public CommandBufferEngine,
      public base::SupportsWeakPtr<GpuScheduler> {
 public:
  GpuScheduler(CommandBuffer* command_buffer,
               gles2::GLES2Decoder* decoder,
               CommandParser* parser);

  virtual ~GpuScheduler();

  void PutChanged();

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules. Whether or not the
  // scheduler is currently scheduled is "reference counted". Every call with
  // false must eventually be paired by a call with true.
  void SetScheduled(bool is_scheduled);

  // Returns whether the scheduler is currently scheduled to process commands.
  bool IsScheduled();

  // Sets a callback that is invoked just before scheduler is rescheduled.
  // Takes ownership of callback object.
  void SetScheduledCallback(Callback0::Type* scheduled_callback);

  // Implementation of CommandBufferEngine.
  virtual Buffer GetSharedMemoryBuffer(int32 shm_id) OVERRIDE;
  virtual void set_token(int32 token) OVERRIDE;
  virtual bool SetGetOffset(int32 offset) OVERRIDE;
  virtual int32 GetGetOffset() OVERRIDE;

  void SetCommandProcessedCallback(Callback0::Type* callback);

  void DeferToFence(base::Closure task);

 private:

  // The GpuScheduler holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GpuScheduler and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

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

  // The GpuScheduler will unschedule itself in the event that further GL calls
  // are issued to it before all these fences have been crossed by the GPU.
  struct UnscheduleFence {
    UnscheduleFence();
    ~UnscheduleFence();

    uint32 fence;
    base::Closure task;
  };
  std::queue<UnscheduleFence> unschedule_fences_;

  scoped_ptr<Callback0::Type> scheduled_callback_;
  scoped_ptr<Callback0::Type> command_processed_callback_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
