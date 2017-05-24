// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "gpu/command_buffer/service/async_api_interface.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/gpu_export.h"

namespace gpu {

union CommandBufferEntry;

// This class schedules commands that have been flushed. They are received via
// a command buffer and forwarded to a command handler.
class GPU_EXPORT CommandExecutor {
 public:
  static const int kParseCommandsSlice = 20;

  CommandExecutor(CommandBufferServiceBase* command_buffer,
                  AsyncAPIInterface* handler);

  ~CommandExecutor();

  bool SetGetBuffer(int32_t transfer_buffer_id);
  void PutChanged();

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules.
  void SetScheduled(bool scheduled);

  bool scheduled() const { return scheduled_; }

  void SetCommandProcessedCallback(const base::Closure& callback);

  using PauseExecutionCallback = base::Callback<bool(void)>;
  void SetPauseExecutionCallback(const PauseExecutionCallback& callback);

 private:
  bool PauseExecution();
  error::Error ProcessCommands(int num_commands);

  // The CommandExecutor holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the CommandExecutor and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBufferServiceBase* command_buffer_;

  // This is used to execute commands.
  AsyncAPIInterface* handler_;

  CommandBufferOffset get_;
  CommandBufferOffset put_;
  volatile CommandBufferEntry* buffer_;
  int32_t entry_count_;

  // Whether the scheduler is currently able to process more commands.
  bool scheduled_ = true;

  base::Closure command_processed_callback_;

  // If this callback returns true, exit PutChanged early.
  PauseExecutionCallback pause_execution_callback_;
  bool paused_ = false;

  DISALLOW_COPY_AND_ASSIGN(CommandExecutor);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_EXECUTOR_H_
