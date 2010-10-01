// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_

#include <vector>

#include "app/surface/transport_dib.h"
#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "gfx/native_widget_types.h"
#include "gfx/size.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

#if defined(OS_MACOSX)
#include "app/surface/accelerated_surface_mac.h"
#endif

namespace gfx {
class GLContext;
}

namespace gpu {

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GPUProcessor : public CommandBufferEngine {
 public:
  explicit GPUProcessor(CommandBuffer* command_buffer);

  // This constructor is for unit tests.
  GPUProcessor(CommandBuffer* command_buffer,
               gles2::GLES2Decoder* decoder,
               CommandParser* parser,
               int commands_per_update);

  virtual ~GPUProcessor();

  // Perform platform specific and common initialization.
  bool Initialize(gfx::PluginWindowHandle hwnd,
                  const gfx::Size& size,
                  const std::vector<int32>& attribs,
                  GPUProcessor* parent,
                  uint32 parent_texture_id);

  // Perform common initialization. Takes ownership of GLContext.
  bool InitializeCommon(gfx::GLContext* context,
                        const gfx::Size& size,
                        const std::vector<int32>& attribs,
                        gles2::GLES2Decoder* parent_decoder,
                        uint32 parent_texture_id);

  void Destroy();
  void DestroyCommon();

  virtual void ProcessCommands();

  // Implementation of CommandBufferEngine.
  virtual Buffer GetSharedMemoryBuffer(int32 shm_id);
  virtual void set_token(int32 token);
  virtual bool SetGetOffset(int32 offset);
  virtual int32 GetGetOffset();

  // Asynchronously resizes an offscreen frame buffer.
  void ResizeOffscreenFrameBuffer(const gfx::Size& size);

#if defined(OS_MACOSX)
  // Needed only on Mac OS X, which does not render into an on-screen
  // window and therefore requires the backing store to be resized
  // manually. Returns an opaque identifier for the new backing store.
  // There are two versions of this method: one for use with the IOSurface
  // available in Mac OS X 10.6; and, one for use with the
  // TransportDIB-based version used on Mac OS X 10.5.
  virtual uint64 SetWindowSizeForIOSurface(const gfx::Size& size);
  virtual TransportDIB::Handle SetWindowSizeForTransportDIB(
      const gfx::Size& size);
  virtual void SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator);
#endif

  // Sets a callback which is called when a SwapBuffers command is processed.
  // Must be called after Initialize().
  // It is not defined on which thread this callback is called.
  virtual void SetSwapBuffersCallback(Callback0::Type* callback);

  // Get the GLES2Decoder associated with this processor.
  gles2::GLES2Decoder* decoder() const { return decoder_.get(); }

 private:
  // Called via a callback just before we are supposed to call the
  // user's swap buffers callback.
  virtual void WillSwapBuffers();

  // The GPUProcessor holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GPUProcessor and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

  int commands_per_update_;

  // TODO(gman): Group needs to be passed in so it can be shared by
  //    multiple GPUProcessors.
  gles2::ContextGroup group_;
  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_ptr<CommandParser> parser_;

#if defined(OS_MACOSX)
  scoped_ptr<AcceleratedSurface> surface_;
#endif

  ScopedRunnableMethodFactory<GPUProcessor> method_factory_;
  scoped_ptr<Callback0::Type> wrapped_swap_buffers_callback_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_H_
