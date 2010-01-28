// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_

#include "app/gfx/native_widget_types.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public base::RefCounted<GPUProcessor>,
                     public CommandBufferEngine {
 public:
  explicit GPUProcessor(CommandBuffer* command_buffer);

  // This constructor is for unit tests.
  GPUProcessor(CommandBuffer* command_buffer,
               gles2::GLES2Decoder* decoder,
               CommandParser* parser,
               int commands_per_update);

  virtual bool Initialize(gfx::PluginWindowHandle hwnd);

  virtual ~GPUProcessor();

  virtual void Destroy();

  virtual void ProcessCommands();

  // Implementation of CommandBufferEngine.
  virtual Buffer GetSharedMemoryBuffer(int32 shm_id);
  virtual void set_token(int32 token);
  virtual bool SetGetOffset(int32 offset);
  virtual int32 GetGetOffset();

 private:
  // The GPUProcessor holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GPUProcessor and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

  scoped_ptr< ::base::SharedMemory> mapped_ring_buffer_;
  int commands_per_update_;

  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_ptr<CommandParser> parser_;
};

}  // namespace gpu

// Callbacks to the GPUProcessor hold a reference count.
template <typename Method>
class CallbackStorage<gpu::GPUProcessor, Method> {
 public:
  CallbackStorage(gpu::GPUProcessor* obj, Method method)
      : obj_(obj),
        meth_(method) {
    DCHECK(obj_);
    obj_->AddRef();
  }

  ~CallbackStorage() {
    obj_->Release();
  }

 protected:
  gpu::GPUProcessor* obj_;
  Method meth_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CallbackStorage);
};

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
