// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the command buffer helper class.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
#define GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_

#include "gpu/command_buffer/common/logging.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/common/command_buffer.h"

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
class CommandBufferHelper {
 public:
  explicit CommandBufferHelper(CommandBuffer* command_buffer);
  virtual ~CommandBufferHelper();

  bool Initialize();

  // Flushes the commands, setting the put pointer to let the buffer interface
  // know that new commands have been added. After a flush returns, the command
  // buffer service is aware of all pending commands and it is guaranteed to
  // have made some progress in processing them. Returns whether the flush was
  // successful. The flush will fail if the command buffer service has
  // disconnected.
  bool Flush();

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

  // Waits for a certain amount of space to be available. Returns address
  // of space.
  CommandBufferEntry* GetSpace(uint32 entries);

  // Typed version of GetSpace. Gets enough room for the given type and returns
  // a reference to it.
  template <typename T>
  T& GetCmdSpace() {
    COMPILE_ASSERT(T::kArgFlags == cmd::kFixed, Cmd_kArgFlags_not_kFixed);
    uint32 space_needed = ComputeNumEntries(sizeof(T));
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T& GetImmediateCmdSpace(size_t data_space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(sizeof(T) + data_space);
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  // Typed version of GetSpace for immediate commands.
  template <typename T>
  T& GetImmediateCmdSpaceTotalSize(size_t total_space) {
    COMPILE_ASSERT(T::kArgFlags == cmd::kAtLeastN, Cmd_kArgFlags_not_kAtLeastN);
    uint32 space_needed = ComputeNumEntries(total_space);
    void* data = GetSpace(space_needed);
    return *reinterpret_cast<T*>(data);
  }

  parse_error::ParseError GetError();

  // Common Commands
  void Noop(uint32 skip_count) {
    cmd::Noop& cmd = GetImmediateCmdSpace<cmd::Noop>(
        skip_count * sizeof(CommandBufferEntry));
    cmd.Init(skip_count);
  }

  void SetToken(uint32 token) {
    cmd::SetToken& cmd = GetCmdSpace<cmd::SetToken>();
    cmd.Init(token);
  }


 private:
  // Waits until get changes, updating the value of get_.
  void WaitForGetChange();

  // Returns the number of available entries (they may not be contiguous).
  int32 AvailableEntries() {
    return (get_ - put_ - 1 + entry_count_) % entry_count_;
  }

  // Synchronize with current service state.
  void SynchronizeState(CommandBuffer::State state);

  CommandBuffer* command_buffer_;
  Buffer ring_buffer_;
  CommandBufferEntry *entries_;
  int32 entry_count_;
  int32 token_;
  int32 last_token_read_;
  int32 get_;
  int32 put_;

  friend class CommandBufferHelperTest;
  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelper);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CMD_BUFFER_HELPER_H_
