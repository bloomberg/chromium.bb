// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_VIDEO_SERVICE_HOST_H_
#define CONTENT_RENDERER_GPU_GPU_VIDEO_SERVICE_HOST_H_

#include "base/memory/singleton.h"
#include "base/threading/non_thread_safe.h"
#include "content/renderer/gpu/gpu_channel_host.h"
#include "ipc/ipc_channel.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_accelerator.h"

class GpuVideoDecodeAcceleratorHost;
namespace gpu {
class CommandBufferHelper;
}

// GpuVideoServiceHost lives on renderer thread and is used to dispatch IPC
// messages to GpuVideoDecodeAcceleratorHost objects.
class GpuVideoServiceHost
    : public IPC::Channel::Listener,
      public base::NonThreadSafe,
      public base::RefCountedThreadSafe<GpuVideoServiceHost> {
 public:
  GpuVideoServiceHost();
  virtual ~GpuVideoServiceHost();

  void set_channel(IPC::SyncChannel* channel);

  // IPC::Channel::Listener implementation; called on render thread because of
  // GpuChannelHost's use of a ChannelProxy/SyncChannel.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // Register a callback to be notified when |*this| can be used to
  // CreateVideo{Decoder,Accelerator} below.  Called on RenderThread.
  // |on_initialized| will get invoked in-line in this function if |*this| is
  // already ready for use, and asynchronously after this function returns
  // otherwise.
  void SetOnInitialized(const base::Closure& on_initialized);

  // Called on RenderThread to create a hardware accelerated video decoder
  // in the GPU process.
  GpuVideoDecodeAcceleratorHost* CreateVideoAccelerator(
      media::VideoDecodeAccelerator::Client* client,
      int32 command_buffer_route_id,
      gpu::CommandBufferHelper* cmd_buffer_helper);

 private:
  // Reference to the channel that the service listens to.
  IPC::SyncChannel* channel_;

  // Router to send messages to a GpuVideoDecoderHost.
  MessageRouter router_;

  // ID for the next GpuVideoDecoderHost.
  int32 next_decoder_host_id_;

  // Callback to invoke when initialized.
  base::Closure on_initialized_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoServiceHost);
};

#endif  // CONTENT_RENDERER_GPU_GPU_VIDEO_SERVICE_HOST_H_
