// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "gpu/command_buffer/common/command_buffer.h"

namespace gpu {

// An object that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class CommandBufferService : public CommandBuffer {
 public:
  CommandBufferService();
  virtual ~CommandBufferService();

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size) OVERRIDE;
  virtual bool Initialize(base::SharedMemory* buffer, int32 size) OVERRIDE;
  virtual Buffer GetRingBuffer() OVERRIDE;
  virtual State GetState() OVERRIDE;
  virtual State GetLastState() OVERRIDE;
  virtual void Flush(int32 put_offset) OVERRIDE;
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE;
  virtual void SetGetOffset(int32 get_offset) OVERRIDE;
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) OVERRIDE;
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual Buffer GetTransferBuffer(int32 handle) OVERRIDE;
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
  virtual void SetParseErrorCallback(const base::Closure& callback);

 private:
  Buffer ring_buffer_;
  int32 num_entries_;
  int32 get_offset_;
  int32 put_offset_;
  base::Closure put_offset_change_callback_;
  base::Closure parse_error_callback_;
  std::vector<Buffer> registered_objects_;
  std::set<int32> unused_registered_object_elements_;
  int32 token_;
  uint32 generation_;
  error::Error error_;
  error::ContextLostReason context_lost_reason_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
