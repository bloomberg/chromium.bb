// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#define CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
#pragma once

#if defined(ENABLE_GPU)

#include <queue>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/process.h"
#include "base/task.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class GpuChannel;
class GpuWatchdog;

class GpuCommandBufferStub
    : public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public base::SupportsWeakPtr<GpuCommandBufferStub> {
 public:
  GpuCommandBufferStub(
      GpuChannel* channel,
      gfx::PluginWindowHandle handle,
      GpuCommandBufferStub* parent,
      const gfx::Size& size,
      const gpu::gles2::DisallowedExtensions& disallowed_extensions,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      uint32 parent_texture_id,
      int32 route_id,
      int32 renderer_id,
      int32 render_view_id,
      GpuWatchdog* watchdog);

  virtual ~GpuCommandBufferStub();

  // IPC::Channel::Listener implementation:
  virtual bool OnMessageReceived(const IPC::Message& message);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Get the GLContext associated with this object.
  gpu::GpuScheduler* scheduler() const { return scheduler_.get(); }

  // Identifies the renderer process.
  int32 renderer_id() const { return renderer_id_; }

  // Identifies a particular renderer belonging to the same renderer process.
  int32 render_view_id() const { return render_view_id_; }

  // Identifies the various GpuCommandBufferStubs in the GPU process belonging
  // to the same renderer process.
  int32 route_id() const { return route_id_; }

#if defined(OS_WIN)
  // Called only by the compositor window's window proc
  void OnCompositorWindowPainted();
#endif  // defined(OS_WIN)

  void ViewResized();

#if defined(OS_MACOSX)
  // Called only by the GpuChannel.
  void AcceleratedSurfaceBuffersSwapped(uint64 swap_buffers_count);
#endif  // defined(OS_MACOSX)

 private:
  // Message handlers:
  void OnInitialize(base::SharedMemoryHandle ring_buffer,
                    int32 size,
                    IPC::Message* reply_message);
  void OnGetState(IPC::Message* reply_message);
  void OnFlush(int32 put_offset,
               int32 last_known_get,
               IPC::Message* reply_message);
  void OnAsyncFlush(int32 put_offset);
  void OnCreateTransferBuffer(int32 size,
                              int32 id_request,
                              IPC::Message* reply_message);
  void OnRegisterTransferBuffer(base::SharedMemoryHandle transfer_buffer,
                                size_t size,
                                int32 id_request,
                                IPC::Message* reply_message);
  void OnDestroyTransferBuffer(int32 id, IPC::Message* reply_message);
  void OnGetTransferBuffer(int32 id, IPC::Message* reply_message);
  void OnResizeOffscreenFrameBuffer(const gfx::Size& size);

  void OnSwapBuffers();
  void OnCommandProcessed();
  void HandleDeferredMessages();
  void OnScheduled();

#if defined(OS_MACOSX)
  void OnSetWindowSize(const gfx::Size& size);
  void SwapBuffersCallback();
#endif  // defined(OS_MACOSX)

  void ResizeCallback(gfx::Size size);
  void ReportState();

  // The lifetime of objects of this class is managed by a GpuChannel. The
  // GpuChannels destroy all the GpuCommandBufferStubs that they own when they
  // are destroyed. So a raw pointer is safe.
  GpuChannel* channel_;

  gfx::PluginWindowHandle handle_;
  base::WeakPtr<GpuCommandBufferStub> parent_;
  gfx::Size initial_size_;
  gpu::gles2::DisallowedExtensions disallowed_extensions_;
  std::string allowed_extensions_;
  std::vector<int32> requested_attribs_;
  uint32 parent_texture_id_;
  int32 route_id_;

  // The following two fields are used on Mac OS X to identify the window
  // for the rendering results on the browser side.
  int32 renderer_id_;
  int32 render_view_id_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GpuScheduler> scheduler_;
  std::queue<IPC::Message*> deferred_messages_;

  GpuWatchdog* watchdog_;
  ScopedRunnableMethodFactory<GpuCommandBufferStub> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferStub);
};

#endif  // defined(ENABLE_GPU)

#endif  // CONTENT_COMMON_GPU_GPU_COMMAND_BUFFER_STUB_H_
