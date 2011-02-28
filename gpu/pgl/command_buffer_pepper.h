// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_PGL_COMMAND_BUFFER_PEPPER_H
#define GPU_PGL_COMMAND_BUFFER_PEPPER_H

#include "gpu/command_buffer/common/command_buffer.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "third_party/npapi/bindings/npapi_extensions.h"
#ifdef __native_client__
#include "native_client/src/third_party/npapi/files/include/npupp.h"
#else
#include "third_party/npapi/bindings/nphostapi.h"
#endif  // __native_client__

namespace {
class SharedMemory;
}

// A CommandBuffer proxy implementation that uses the Pepper API to access
// the command buffer.

class CommandBufferPepper : public gpu::CommandBuffer {
 public:
  CommandBufferPepper(NPP npp,
                      NPDevice* device,
                      NPDeviceContext3D* device_context);
  virtual ~CommandBufferPepper();

  // CommandBuffer implementation.
  virtual bool Initialize(int32 size);
  virtual bool Initialize(base::SharedMemory* buffer, int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);

  gpu::error::Error GetCachedError();

 private:
  CommandBuffer::State ConvertState();

  NPP npp_;
  NPDevice* device_;
  NPDeviceContext3D* context_;
};

#endif  // GPU_PGL_COMMAND_BUFFER_PEPPER_H
