// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"

namespace gpu {

class GPU_EXPORT TransferBufferManagerInterface {
 public:
  virtual ~TransferBufferManagerInterface();

  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) = 0;
  virtual int32 RegisterTransferBuffer(
      base::SharedMemory* shared_memory,
      size_t size,
      int32 id_request) = 0;
  virtual void DestroyTransferBuffer(int32 id) = 0;
  virtual Buffer GetTransferBuffer(int32 handle) = 0;

};

class GPU_EXPORT TransferBufferManager
    : public TransferBufferManagerInterface {
 public:
  TransferBufferManager();

  bool Initialize();
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request) OVERRIDE;
  virtual int32 RegisterTransferBuffer(
      base::SharedMemory* shared_memory,
      size_t size,
      int32 id_request) OVERRIDE;
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE;
  virtual Buffer GetTransferBuffer(int32 handle) OVERRIDE;

 private:
  virtual ~TransferBufferManager();

  std::set<int32> unused_registered_object_elements_;
  std::vector<Buffer> registered_objects_;
  size_t shared_memory_bytes_allocated_;

  DISALLOW_COPY_AND_ASSIGN(TransferBufferManager);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TRANSFER_BUFFER_MANAGER_H_
