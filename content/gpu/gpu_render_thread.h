// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_RENDER_THREAD_H_
#define CONTENT_GPU_GPU_RENDER_THREAD_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
#include "content/common/child_thread.h"
#include "content/common/gpu_info.h"
#include "content/gpu/gpu_channel.h"
#include "content/gpu/gpu_config.h"
#include "content/gpu/x_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "ui/gfx/native_widget_types.h"

namespace IPC {
struct ChannelHandle;
}

// A GpuRenderThread is a thread responsible for issuing rendering commands to
// a GPU. There is currently only one per GPU process. This might change. Assume
// there are many, all running on different threads.
//
// A GpuRenderThread can also be hosted in the browser process in single process
// or in-process GPU modes. In this case there is no corresponding
// GpuChildThread and this is the reason the GpuChildThread is referenced via
// a pointer to IPC::Message::Sender, which can be implemented by other hosts
// to send IPC messages to the browser process IO thread on the
// GpuRenderThread's behalf.
class GpuRenderThread : public IPC::Channel::Listener,
                        public IPC::Message::Sender {
 public:
  GpuRenderThread(IPC::Message::Sender* browser_channel,
                  GpuWatchdogThread* gpu_watchdog_thread,
                  MessageLoop* io_message_loop,
                  base::WaitableEvent* shutdown_event);
  ~GpuRenderThread();

  // Remove the channel for a particular renderer.
  void RemoveChannel(int renderer_id);

  // Listener overrides.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // Sender overrides.
  virtual bool Send(IPC::Message* msg);

 private:
  // Message handlers.
  void OnEstablishChannel(int renderer_id);
  void OnCloseChannel(const IPC::ChannelHandle& channel_handle);
  void OnSynchronize();
  void OnCreateViewCommandBuffer(
      gfx::PluginWindowHandle window,
      int32 render_view_id,
      int32 renderer_id,
      const GPUCreateCommandBufferConfig& init_params);
#if defined(OS_MACOSX)
  void OnAcceleratedSurfaceBuffersSwappedACK(
      int renderer_id, int32 route_id, uint64 swap_buffers_count);
  void OnDestroyCommandBuffer(int renderer_id, int32 renderer_view_id);
#endif

  MessageLoop* io_message_loop_;
  base::WaitableEvent* shutdown_event_;

  // Either an IPC channel to the browser or, if the GpuRenderThread is
  // running in the browser process, a Sender implementation that will post
  // IPC messages to the UI thread.
  IPC::Message::Sender* browser_channel_;

  // These objects manage channels to individual renderer processes there is
  // one channel for each renderer process that has connected to this GPU
  // process.
  typedef base::hash_map<int, scoped_refptr<GpuChannel> > GpuChannelMap;
  GpuChannelMap gpu_channels_;
  GpuWatchdogThread* watchdog_thread_;

  DISALLOW_COPY_AND_ASSIGN(GpuRenderThread);
};

#endif  // CONTENT_GPU_GPU_RENDER_THREAD_H_
