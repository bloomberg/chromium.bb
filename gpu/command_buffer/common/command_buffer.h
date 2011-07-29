// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_

#include "../common/buffer.h"
#include "../common/constants.h"

namespace base {
class SharedMemory;
}

namespace gpu {

// Common interface for CommandBuffer implementations.
class CommandBuffer {
 public:
  enum {
    kMaxCommandBufferSize = 4 * 1024 * 1024
  };

  struct State {
    State()
        : num_entries(0),
          get_offset(0),
          put_offset(0),
          token(-1),
          error(error::kNoError),
          context_lost_reason(error::kUnknown),
          generation(0) {
    }

    // Size of the command buffer in command buffer entries.
    int32 num_entries;

    // The offset (in entries) from which the reader is reading.
    int32 get_offset;

    // The offset (in entries) at which the writer is writing.
    int32 put_offset;

    // The current token value. This is used by the writer to defer
    // changes to shared memory objects until the reader has reached a certain
    // point in the command buffer. The reader is responsible for updating the
    // token value, for example in response to an asynchronous set token command
    // embedded in the command buffer. The default token value is zero.
    int32 token;

    // Error status.
    error::Error error;

    // Lost context detail information.
    error::ContextLostReason context_lost_reason;

    // Generation index of this state. The generation index is incremented every
    // time a new state is retrieved from the command processor, so that
    // consistency can be kept even if IPC messages are processed out-of-order.
    uint32 generation;
  };

  CommandBuffer() {
  }

  virtual ~CommandBuffer() {
  }

  // Initialize the command buffer with the given size.
  virtual bool Initialize(int32 size) = 0;

  // Initialize the command buffer using the given preallocated buffer.
  virtual bool Initialize(base::SharedMemory* buffer, int32 size) = 0;

  // Gets the ring buffer for the command buffer.
  virtual Buffer GetRingBuffer() = 0;

  // Returns the current status.
  virtual State GetState() = 0;

  // Returns the last state without synchronizing with the service.
  virtual State GetLastState() = 0;

  // The writer calls this to update its put offset. This ensures the reader
  // sees the latest added commands, and will eventually process them. On the
  // service side, commands are processed up to the given put_offset before
  // subsequent Flushes on the same GpuChannel.
  virtual void Flush(int32 put_offset) = 0;

  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until all pending commands
  // have been executed.
  virtual State FlushSync(int32 put_offset, int32 last_known_get) = 0;

  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32 get_offset) = 0;

  // Create a transfer buffer and return a handle that uniquely
  // identifies it or -1 on error. id_request lets the caller request a
  // specific id for the transfer buffer, or -1 if the caller does not care.
  // If the requested id can not be fulfilled, a different id will be returned.
  // id_request must be either -1 or between 0 and 100.
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) = 0;

  // Register an existing shared memory object and get an ID that can be used
  // to identify it in the command buffer. Callee dups the handle until
  // DestroyTransferBuffer is called. id_request lets the caller request a
  // specific id for the transfer buffer, or -1 if the caller does not care.
  // If the requested id can not be fulfilled, a different id will be returned.
  // id_request must be either -1 or between 0 and 100.
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request) = 0;

  // Destroy a transfer buffer and recycle the handle.
  virtual void DestroyTransferBuffer(int32 id) = 0;

  // Get the transfer buffer associated with a handle.
  virtual Buffer GetTransferBuffer(int32 handle) = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32 token) = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(error::Error) = 0;

  // Allows the reader to set the current context lost reason.
  // NOTE: if calling this in conjunction with SetParseError,
  // call this first.
  virtual void SetContextLostReason(error::ContextLostReason) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
