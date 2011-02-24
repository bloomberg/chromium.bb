// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
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
  virtual bool Initialize(int32 size);
  virtual Buffer GetRingBuffer();
  virtual State GetState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(error::Error error);

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
  virtual void SetPutOffsetChangeCallback(Callback0::Type* callback);

 private:
  scoped_ptr< base::SharedMemory> ring_buffer_;
  int32 num_entries_;
  int32 get_offset_;
  int32 put_offset_;
  scoped_ptr<Callback0::Type> put_offset_change_callback_;
  std::vector<Buffer> registered_objects_;
  std::set<int32> unused_registered_object_elements_;
  int32 token_;
  error::Error error_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_SERVICE_H_
