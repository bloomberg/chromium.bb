// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_

#include "base/shared_memory.h"
#include "base/task.h"

namespace gpu {

// Common interface for CommandBuffer implementations.
class CommandBuffer {
 public:
  CommandBuffer() {
  }

  virtual ~CommandBuffer() {
  }

  // Initialize the command buffer with the given size (number of command
  // entries).
  virtual base::SharedMemory* Initialize(int32 size) = 0;

  // Gets the shared memory ring buffer object for the command buffer.
  virtual base::SharedMemory* GetRingBuffer() = 0;

  virtual int32 GetSize() = 0;

  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until after the put offset
  // change callback has been invoked. Returns -1 if the put offset is invalid.
  virtual int32 SyncOffsets(int32 put_offset) = 0;

  // Returns the current get offset. This can be called from any thread.
  virtual int32 GetGetOffset() = 0;

  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32 get_offset) = 0;

  // Returns the current put offset. This can be called from any thread.
  virtual int32 GetPutOffset() = 0;

  // Sets a callback that should be posted on another thread whenever the put
  // offset is changed. The callback must not return until some progress has
  // been made (unless the command buffer is empty), i.e. the
  // get offset must have changed. It need not process the entire command
  // buffer though. This allows concurrency between the writer and the reader
  // while giving the writer a means of waiting for the reader to make some
  // progress before attempting to write more to the command buffer. Avoiding
  // the use of a synchronization primitive like a condition variable to
  // synchronize reader and writer reduces the risk of deadlock.
  // Takes ownership of callback. The callback is invoked on the plugin thread.
  virtual void SetPutOffsetChangeCallback(Callback0::Type* callback) = 0;

  // Create a shared memory transfer buffer and return a handle that uniquely
  // identifies it or -1 on error.
  virtual int32 CreateTransferBuffer(size_t size) = 0;

  // Destroy a shared memory transfer buffer and recycle the handle.
  virtual void DestroyTransferBuffer(int32 id) = 0;

  // Get the shared memory associated with a handle.
  virtual base::SharedMemory* GetTransferBuffer(int32 handle) = 0;

  // Get the current token value. This is used for by the writer to defer
  // changes to shared memory objects until the reader has reached a certain
  // point in the command buffer. The reader is responsible for updating the
  // token value, for example in response to an asynchronous set token command
  // embedded in the command buffer. The default token value is zero.
  virtual int32 GetToken() = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32 token) = 0;

  // Get the current parse error and reset it to zero. Zero means no error. Non-
  // zero means error. The default error status is zero.
  virtual int32 ResetParseError() = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(int32 parse_error) = 0;

  // Returns whether the command buffer is in the error state.
  virtual bool GetErrorStatus() = 0;

  // Allows the reader to set the error status. Once in an error state, the
  // command buffer cannot recover and ceases to process commands.
  virtual void RaiseErrorStatus() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
