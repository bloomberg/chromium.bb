// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_

#include "../../gpu_export.h"
#include "../common/buffer.h"
#include "../common/constants.h"

namespace base {
class SharedMemory;
}

namespace gpu {

// Common interface for CommandBuffer implementations.
class GPU_EXPORT CommandBuffer {
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

  struct ConsoleMessage {
    // An user supplied id.
    int32 id;
    // The message.
    std::string message;
  };

  CommandBuffer() {
  }

  virtual ~CommandBuffer() {
  }

  // Initialize the command buffer with the given size.
  virtual bool Initialize() = 0;

  // Returns the current status.
  virtual State GetState() = 0;

  // Returns the last state without synchronizing with the service.
  virtual State GetLastState() = 0;

  // Returns the last token without synchronizing with the service. Note that
  // while you could just call GetLastState().token, GetLastState needs to be
  // fast as it is called for every command where GetLastToken is only called
  // by code that needs to know the last token so it can be slower but more up
  // to date than GetLastState.
  virtual int32 GetLastToken() = 0;

  // The writer calls this to update its put offset. This ensures the reader
  // sees the latest added commands, and will eventually process them. On the
  // service side, commands are processed up to the given put_offset before
  // subsequent Flushes on the same GpuChannel.
  virtual void Flush(int32 put_offset) = 0;

  // The writer calls this to update its put offset. This function returns the
  // reader's most recent get offset. Does not return until all pending commands
  // have been executed.
  virtual State FlushSync(int32 put_offset, int32 last_known_get) = 0;

  // Sets the buffer commands are read from.
  // Also resets the get and put offsets to 0.
  virtual void SetGetBuffer(int32 transfer_buffer_id) = 0;

  // Sets the current get offset. This can be called from any thread.
  virtual void SetGetOffset(int32 get_offset) = 0;

  // Create a transfer buffer of the given size. Returns its ID or -1 on
  // error.
  virtual Buffer CreateTransferBuffer(size_t size, int32* id) = 0;

  // Destroy a transfer buffer. The ID must be positive.
  virtual void DestroyTransferBuffer(int32 id) = 0;

  // Get the transfer buffer associated with an ID. Returns a null buffer for
  // ID 0.
  virtual Buffer GetTransferBuffer(int32 id) = 0;

  // Allows the reader to update the current token value.
  virtual void SetToken(int32 token) = 0;

  // Allows the reader to set the current parse error.
  virtual void SetParseError(error::Error) = 0;

  // Allows the reader to set the current context lost reason.
  // NOTE: if calling this in conjunction with SetParseError,
  // call this first.
  virtual void SetContextLostReason(error::ContextLostReason) = 0;

// The NaCl Win64 build only really needs the struct definitions above; having
// GetLastError declared would mean we'd have to also define it, and pull more
// of gpu in to the NaCl Win64 build.
#if !defined(NACL_WIN64)
  // TODO(apatrick): this is a temporary optimization while skia is calling
  // RendererGLContext::MakeCurrent prior to every GL call. It saves returning 6
  // ints redundantly when only the error is needed for the CommandBufferProxy
  // implementation.
  virtual error::Error GetLastError();
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(CommandBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_COMMAND_BUFFER_H_
