// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_PGL_COMMAND_BUFFER_PEPPER_H
#define GPU_PGL_COMMAND_BUFFER_PEPPER_H

#include "gpu/command_buffer/common/command_buffer.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npruntime.h"
#include "webkit/glue/plugins/nphostapi.h"

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
  virtual gpu::Buffer GetRingBuffer();
  virtual int32 GetSize();
  virtual int32 SyncOffsets(int32 put_offset);
  virtual int32 GetGetOffset();
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 GetPutOffset();
  virtual int32 CreateTransferBuffer(size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual int32 GetToken();
  virtual void SetToken(int32 token);
  virtual int32 ResetParseError();
  virtual void SetParseError(int32 parse_error);
  virtual bool GetErrorStatus();
  virtual void RaiseErrorStatus();

 private:
  NPP npp_;
  NPDevice* device_;
  NPDeviceContext3D* context_;
};

#endif  // GPU_PGL_COMMAND_BUFFER_PEPPER_H


