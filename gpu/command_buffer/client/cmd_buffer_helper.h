// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the command buffer helper class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_

#include <string.h>
#include <time.h>

#include "../../gpu_export.h"
#include "../common/logging.h"
#include "../common/constants.h"
#include "../common/cmd_buffer_common.h"
#include "../common/command_buffer.h"

namespace gpu {

// Command buffer helper class. This class simplifies ring buffer management:
// it will allocate the buffer, give it to the buffer interface, and let the
// user add commands to it, while taking care of the synchronization (put and
// get). It also provides a way to ensure commands have been executed, through
// the token mechanism:
//
// helper.AddCommand(...);
// helper.AddCommand(...);
// int32 token = helper.InsertToken();
// helper.AddCommand(...);
// helper.AddCommand(...);
// [...]
//
// helper.WaitForToken(token);  // this doesn't return until the first two
//                              // commands have been executed.
class GPU_EXPORT CommandBufferHelper {
 public:
  explicit CommandBufferHelper(CommandBuffer* command_buffer);
  virtual ~CommandBufferHelper();

  // Initializes the CommandBufferHelper.
  // Parameters:
  //   ring_buffer_size: The size of the ring buffer portion of the command
  //       buffer.
  bool Initialize(int32 ring_buffer_size);

  // Asynchronously flushes the commands, setting the put pointer to let the
  // buffer interface know that new commands have been added. After a flush
  // returns, the command buffer service is aware of all pending commands.
  void Flush();

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added. After a flush returns, the command
  // buffer service is aware of all pending commands and it is guaranteed to
  // have made some progress in processing them. Returns whether the flush was
  // successful. The flush will fail if the command buffer service has
  // disconnected.
  bool FlushSync();

  // Waits until all the commands have been executed. Returns whether it
  // was successful. The function will fail if the command buffer service has
  // disconnected.
  bool Finish();

  // Waits until a given number of available entries are available.
  // Parameters:
  //   count: number of entries needed. This value must be at most
  //     the size of the buffer minus one.
  void WaitForAvailableEntries(int32 count);

  // Inserts a new token into the command buffer. This token either has a value
  // different from previously inserted tokens, or ensures that previously
  // inserted tokens with that value have already passed through the command
  // stream.
  // Returns:
  //   the value of the new token or -1 if the command buffer reader has
  //   shutdown.
  int32 InsertToken();

  // Waits until the token of a particular value has passed through the command
  // stream (i.e. commands inserted before that token have been executed).
  // NOTE: This will call Flush if it needs to block.
  // Parameters:
  //   the value of the token to wait for.
  void WaitForToken(int32 token);

  // Called prior to each command being issued. Waits for a certain amount of
  // space to be available. Returns address of space.
  CommandBufferEntry* GetSpace(uint32 entries);

  // Typed version of GetSpace. Gets enough room for the given type and returns
  // a reference to it.
  template <typename T>
  T* GetCmdSpace() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    uint32 space_needed = ComputeNumEntries(sizeof(T));
    void* data = GetSpace(space_needed);
    return reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T* GetImmediateCmdSpace(size_t data_space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(sizeof(T) + data_space);
    void* data = GetSpace(space_needed);
    return reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T* GetImmediateCmdSpaceTotalSize(size_t total_space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(total_space);
    void* data = GetSpace(space_needed);
    return reinterpret_cast<T*>(data);
  }

  int32 last_token_read() const {
    return command_buffer_->GetLastState().token;
  }

  int32 get_offset() const {
    return command_buffer_->GetLastState().get_offset;
  }

  error::Error GetError();

  // Common Commands
  void Noop(uint32 skip_count) {
    cmd::Noop* cmd = GetImmediateCmdSpace<cmd::Noop>(
        (skip_count - 1) * sizeof(CommandBufferEntry));
    if (cmd) {
      cmd->Init(skip_count);
    }
  }

  void SetToken(uint32 token) {
    cmd::SetToken* cmd = GetCmdSpace<cmd::SetToken>();
    if (cmd) {
      cmd->Init(token);
    }
  }

  void Jump(uint32 offset) {
    cmd::Jump* cmd = GetCmdSpace<cmd::Jump>();
    if (cmd) {
      cmd->Init(offset);
    }
  }

  void JumpRelative(int32 offset) {
    cmd::JumpRelative* cmd = GetCmdSpace<cmd::JumpRelative>();
    if (cmd) {
      cmd->Init(offset);
    }
  }

  void Call(uint32 offset) {
    cmd::Call* cmd = GetCmdSpace<cmd::Call>();
    if (cmd) {
      cmd->Init(offset);
    }
  }

  void CallRelative(int32 offset) {
    cmd::CallRelative* cmd = GetCmdSpace<cmd::CallRelative>();
    if (cmd) {
      cmd->Init(offset);
    }
  }

  void Return() {
    cmd::Return* cmd = GetCmdSpace<cmd::Return>();
    if (cmd) {
      cmd->Init();
    }
  }

  void SetBucketSize(uint32 bucket_id, uint32 size) {
    cmd::SetBucketSize* cmd = GetCmdSpace<cmd::SetBucketSize>();
    if (cmd) {
      cmd->Init(bucket_id, size);
    }
  }

  void SetBucketData(uint32 bucket_id,
                     uint32 offset,
                     uint32 size,
                     uint32 shared_memory_id,
                     uint32 shared_memory_offset) {
    cmd::SetBucketData* cmd = GetCmdSpace<cmd::SetBucketData>();
    if (cmd) {
      cmd->Init(bucket_id,
                offset,
                size,
                shared_memory_id,
                shared_memory_offset);
    }
  }

  void SetBucketDataImmediate(
      uint32 bucket_id, uint32 offset, const void* data, uint32 size) {
    cmd::SetBucketDataImmediate* cmd =
        GetImmediateCmdSpace<cmd::SetBucketDataImmediate>(size);
    if (cmd) {
      cmd->Init(bucket_id, offset, size);
      memcpy(ImmediateDataAddress(cmd), data, size);
    }
  }

  void GetBucketStart(uint32 bucket_id,
                      uint32 result_memory_id,
                      uint32 result_memory_offset,
                      uint32 data_memory_size,
                      uint32 data_memory_id,
                      uint32 data_memory_offset) {
    cmd::GetBucketStart* cmd = GetCmdSpace<cmd::GetBucketStart>();
    if (cmd) {
      cmd->Init(bucket_id,
                result_memory_id,
                result_memory_offset,
                data_memory_size,
                data_memory_id,
                data_memory_offset);
    }
  }

  void GetBucketData(uint32 bucket_id,
                     uint32 offset,
                     uint32 size,
                     uint32 shared_memory_id,
                     uint32 shared_memory_offset) {
    cmd::GetBucketData* cmd = GetCmdSpace<cmd::GetBucketData>();
    if (cmd) {
      cmd->Init(bucket_id,
                offset,
                size,
                shared_memory_id,
                shared_memory_offset);
    }
  }

  CommandBuffer* command_buffer() const {
    return command_buffer_;
  }

  Buffer get_ring_buffer() const {
    return ring_buffer_;
  }

  void FreeRingBuffer();

  bool HaveRingBuffer() const {
    return ring_buffer_id_ != -1;
  }

  bool usable () const {
    return usable_;
  }

  void ClearUsable() {
    usable_ = false;
  }

 private:
  // Waits until get changes, updating the value of get_.
  void WaitForGetChange();

  // Returns the number of available entries (they may not be contiguous).
  int32 AvailableEntries() {
    return (get_offset() - put_ - 1 + usable_entry_count_) %
        usable_entry_count_;
  }

  bool AllocateRingBuffer();

  CommandBuffer* command_buffer_;
  int32 ring_buffer_id_;
  int32 ring_buffer_size_;
  Buffer ring_buffer_;
  CommandBufferEntry* entries_;
  int32 total_entry_count_;  // the total number of entries
  int32 usable_entry_count_;  // the usable number (ie, minus space for jump)
  int32 token_;
  int32 put_;
  int32 last_put_sent_;
  int commands_issued_;
  bool usable_;

  // Using C runtime instead of base because this file cannot depend on base.
  clock_t last_flush_time_;

  friend class CommandBufferHelperTest;
  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelper);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
