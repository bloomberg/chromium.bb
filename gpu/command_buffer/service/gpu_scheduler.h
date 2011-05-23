// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_

#include <queue>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/task.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/service/cmd_buffer_engine.h"
#include "gpu/command_buffer/service/cmd_parser.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/transport_dib.h"

#if defined(OS_MACOSX)
#include "ui/gfx/surface/accelerated_surface_mac.h"
#endif

namespace gfx {
class GLContext;
class GLSurface;
}

namespace gpu {
namespace gles2 {
class ContextGroup;
}

// This class processes commands in a command buffer. It is event driven and
// posts tasks to the current message loop to do additional work.
class GpuScheduler : public CommandBufferEngine {
 public:
  // If a group is not passed in one will be created.
  GpuScheduler(CommandBuffer* command_buffer, gles2::ContextGroup* group);

  // This constructor is for unit tests.
  GpuScheduler(CommandBuffer* command_buffer,
               gles2::GLES2Decoder* decoder,
               CommandParser* parser,
               int commands_per_update);

  virtual ~GpuScheduler();

  // Perform platform specific and common initialization.
  bool Initialize(gfx::PluginWindowHandle hwnd,
                  const gfx::Size& size,
                  const gles2::DisallowedExtensions& disallowed_extensions,
                  const char* allowed_extensions,
                  const std::vector<int32>& attribs,
                  GpuScheduler* parent,
                  uint32 parent_texture_id);

  void Destroy();
  void DestroyCommon();

  void PutChanged(bool sync);

  // Sets whether commands should be processed by this scheduler. Setting to
  // false unschedules. Setting to true reschedules. Whether or not the
  // scheduler is currently scheduled is "reference counted". Every call with
  // false must eventually be paired by a call with true.
  void SetScheduled(bool is_scheduled);

  // Returns whether the scheduler is currently scheduled to process commands.
  bool IsScheduled();

  // Sets a callback that is invoked just before scheduler is rescheduled.
  // Takes ownership of callback object.
  void SetScheduledCallback(Callback0::Type* scheduled_callback);

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
  // Returns the id of the current IOSurface, or 0.
  virtual uint64 GetSurfaceId();
  // To prevent the GPU process from overloading the browser process,
  // we need to track the number of swap buffers calls issued and
  // acknowledged per on-screen (IOSurface-backed) context, and keep
  // the GPU from getting too far ahead of the browser. Note that this
  // is also predicated on a flow control mechanism between the
  // renderer and GPU processes.
  uint64 swap_buffers_count() const;
  uint64 acknowledged_swap_buffers_count() const;
  void set_acknowledged_swap_buffers_count(
      uint64 acknowledged_swap_buffers_count);

  void DidDestroySurface();
#endif

  // Sets a callback that is called when a glResizeCHROMIUM command
  // is processed.
  void SetResizeCallback(Callback1<gfx::Size>::Type* callback);

  // Sets a callback which is called when a SwapBuffers command is processed.
  // Must be called after Initialize().
  // It is not defined on which thread this callback is called.
  void SetSwapBuffersCallback(Callback0::Type* callback);

  void SetCommandProcessedCallback(Callback0::Type* callback);

  // Sets a callback which is called after a Set/WaitLatch command is processed.
  // The bool parameter will be true for SetLatch, and false for a WaitLatch
  // that is blocked. An unblocked WaitLatch will not trigger a callback.
  void SetLatchCallback(const base::Callback<void(bool)>& callback) {
    decoder_->SetLatchCallback(callback);
  }

  // Get the GLES2Decoder associated with this scheduler.
  gles2::GLES2Decoder* decoder() const { return decoder_.get(); }

 protected:
  // Perform common initialization. Takes ownership of GLSurface and GLContext.
  bool InitializeCommon(
      gfx::GLSurface* surface,
      gfx::GLContext* context,
      const gfx::Size& size,
      const gles2::DisallowedExtensions& disallowed_extensions,
      const char* allowed_extensions,
      const std::vector<int32>& attribs,
      gles2::GLES2Decoder* parent_decoder,
      uint32 parent_texture_id);


 private:
  // Helper which causes a call to ProcessCommands to be scheduled later.
  void ScheduleProcessCommands();

  // Called via a callback just before we are supposed to call the
  // user's swap buffers callback.
  void WillSwapBuffers();
  void ProcessCommands();

  // The GpuScheduler holds a weak reference to the CommandBuffer. The
  // CommandBuffer owns the GpuScheduler and holds a strong reference to it
  // through the ProcessCommands callback.
  CommandBuffer* command_buffer_;

  int commands_per_update_;

  scoped_ptr<gles2::GLES2Decoder> decoder_;
  scoped_ptr<CommandParser> parser_;

  // Greater than zero if this is waiting to be rescheduled before continuing.
  int unscheduled_count_;

  scoped_ptr<Callback0::Type> scheduled_callback_;

#if defined(OS_MACOSX)
  scoped_ptr<AcceleratedSurface> surface_;
  uint64 swap_buffers_count_;
  uint64 acknowledged_swap_buffers_count_;
#endif

  ScopedRunnableMethodFactory<GpuScheduler> method_factory_;
  scoped_ptr<Callback0::Type> wrapped_swap_buffers_callback_;
  scoped_ptr<Callback0::Type> command_processed_callback_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_SCHEDULER_H_
