// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_

#include "../common/buffer.h"
#include "../common/constants.h"

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
          error(error::kNoError) {
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
  };

  CommandBuffer() {
  }

  virtual ~CommandBuffer() {
  }

  // Initialize the command buffer with the given size.
  virtual bool Initialize(int32 size) = 0;

  // Gets the ring buffer for the command buffer.
  virtual Buffer GetRingBuffer() = 0;

  // Returns the current status.
  virtual State GetState() = 0;

  // The writer calls this to update its put offset. This ensures the reader
  // sees the latest added commands, and will eventually process them.
  virtual void Flush(int32 put_offset) = 0;

  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until after the put offset
  // change callback has been invoked. Returns -1 if the put offset is invalid.
  // As opposed to Flush(), this function guarantees that the reader has
  // processed some commands before returning (assuming the command buffer isn't
  // empty and there is no error).
  virtual State FlushSync(int32 put_offset) = 0;

  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32 get_offset) = 0;

  // Create a transfer buffer and return a handle that uniquely
  // identifies it or -1 on error.
  virtual int32 CreateTransferBuffer(size_t size) = 0;

  // Register an existing shared memory object and get an ID that can be used
  // to identify it in the command buffer. Callee dups the handle until
  // DestroyTransferBuffer is called.
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size) = 0;

  // Destroy a transfer buffer and recycle the handle.
  virtual void DestroyTransferBuffer(int32 id) = 0;

  // Get the transfer buffer associated with a handle.
  virtual Buffer GetTransferBuffer(int32 handle) = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32 token) = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(error::Error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
