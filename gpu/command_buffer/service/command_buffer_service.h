// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_

#include "base/callback.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"

namespace gpu {

class TransferBufferManagerInterface;

// An object that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class GPU_EXPORT CommandBufferService : public CommandBuffer {
 public:
  typedef base::Callback<bool(int32)> GetBufferChangedCallback;
  explicit CommandBufferService(
      TransferBufferManagerInterface* transfer_buffer_manager);
  virtual ~CommandBufferService();

  // CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual int32 GetLastToken() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetBuffer(int32 transfer_buffer_id) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual Buffer CreateTransferBuffer(size_t size, int32* id) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual Buffer GetTransferBuffer(int32 id) OVERRIDE;
  virtual void SetToken(int32 token) OVERRIDE;
  virtual void SetParseError(error::Error error) OVERRIDE;
  virtual void SetContextLostReason(error::ContextLostReason) OVERRIDE;

  // Sets a callback that is called whenever the put offset is changed. When
  // called with sync==true, the callback must not return until some progress
  // has been made (unless the command buffer is empty), i.e. the get offset
  // must have changed. It need not process the entire command buffer though.
  // This allows concurrency between the writer and the reader while giving the
  // writer a means of waiting for the reader to make some progress before
  // attempting to write more to the command buffer. Takes ownership of
  // callback.
  virtual void SetPutOffsetChangeCallback(const base::Closure& callback);
  // Sets a callback that is called whenever the get buffer is changed.
  virtual void SetGetBufferChangeCallback(
      const GetBufferChangedCallback& callback);
  virtual void SetParseErrorCallback(const base::Closure& callback);

  // Setup the shared memory that shared state should be copied into.
  bool SetSharedStateBuffer(scoped_ptr<base::SharedMemory> shared_state_shm);

  // Copy the current state into the shared state transfer buffer.
  void UpdateState();

  // Register an existing shared memory object and get an ID that can be used
  // to identify it in the command buffer. Callee dups the handle until
  // DestroyTransferBuffer is called.
  bool RegisterTransferBuffer(int32 id,
                              base::SharedMemory* shared_memory,
                              size_t size);

 private:
  int32 ring_buffer_id_;
  Buffer ring_buffer_;
  scoped_ptr<base::SharedMemory> shared_state_shm_;
  CommandBufferSharedState* shared_state_;
  int32 num_entries_;
  int32 get_offset_;
  int32 put_offset_;
  base::Closure put_offset_change_callback_;
  GetBufferChangedCallback get_buffer_change_callback_;
  base::Closure parse_error_callback_;
  TransferBufferManagerInterface* transfer_buffer_manager_;
  int32 token_;
  uint32 generation_;
  error::Error error_;
  error::ContextLostReason context_lost_reason_;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferService);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
