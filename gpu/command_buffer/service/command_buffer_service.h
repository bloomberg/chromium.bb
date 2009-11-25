// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_H_

#include <set>
#include <vector>

#include "base/linked_ptr.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/np_utils/default_np_object.h"
#include "gpu/np_utils/np_dispatcher.h"

namespace command_buffer {

// An NPObject that implements a shared memory command buffer and a synchronous
// API to manage the put and get pointers.
class CommandBufferService : public CommandBuffer {
 public:
  CommandBufferService();
  virtual ~CommandBufferService();

  // Overrides CommandBuffer.
  virtual bool Initialize(::base::SharedMemory* ring_buffer);

  // Overrides CommandBuffer.
  virtual ::base::SharedMemory* GetRingBuffer();

  virtual int32 GetSize();

  // Overrides CommandBuffer.
  virtual int32 SyncOffsets(int32 put_offset);

  // Overrides CommandBuffer.
  virtual int32 GetGetOffset();

  // Overrides CommandBuffer.
  virtual void SetGetOffset(int32 get_offset);

  // Overrides CommandBuffer.
  virtual int32 GetPutOffset();

  // Overrides CommandBuffer.
  virtual void SetPutOffsetChangeCallback(Callback0::Type* callback);

  // Overrides CommandBuffer.
  virtual int32 CreateTransferBuffer(size_t size);

  // Overrides CommandBuffer.
  virtual void DestroyTransferBuffer(int32 id);

  // Overrides CommandBuffer.
  virtual ::base::SharedMemory* GetTransferBuffer(int32 handle);

  // Overrides CommandBuffer.
  virtual int32 GetToken();

  // Overrides CommandBuffer.
  virtual void SetToken(int32 token);

  // Overrides CommandBuffer.
  virtual int32 ResetParseError();

  // Overrides CommandBuffer.
  virtual void SetParseError(int32 parse_error);

  // Overrides CommandBuffer.
  virtual bool GetErrorStatus();

  // Overrides CommandBuffer.
  virtual void RaiseErrorStatus();

 private:
  scoped_ptr< ::base::SharedMemory> ring_buffer_;
  int32 size_;
  int32 get_offset_;
  int32 put_offset_;
  scoped_ptr<Callback0::Type> put_offset_change_callback_;
  std::vector<linked_ptr< ::base::SharedMemory> > registered_objects_;
  std::set<int32> unused_registered_object_elements_;
  int32 token_;
  int32 parse_error_;
  bool error_status_;
};

}  // namespace command_buffer

#endif  // GPU_COMMAND_BUFFER_SERVICE_COMMAND_BUFFER_H_
