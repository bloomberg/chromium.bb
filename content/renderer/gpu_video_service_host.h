// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
#define CONTENT_RENDERER_GPU_VIDEO_SERVICE_HOST_H_

#include "base/memory/singleton.h"
#include "content/renderer/gpu_channel_host.h"
#include "content/renderer/gpu_video_decoder_host.h"
#include "ipc/ipc_channel.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"
#include "media/video/video_decode_accelerator.h"

class GpuVideoDecodeAcceleratorHost;

// GpuVideoServiceHost lives on IO thread and is used to dispatch IPC messages
// to GpuVideoDecoderHost objects.
class GpuVideoServiceHost : public IPC::ChannelProxy::MessageFilter {
 public:
  GpuVideoServiceHost();
  virtual ~GpuVideoServiceHost();

  // IPC::ChannelProxy::MessageFilter implementations, called on IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();
  virtual void OnChannelClosing();

  // Register a callback to be notified when |*this| can be used to
  // CreateVideo{Decoder,Accelerator} below.  Called on RenderThread.
  // |on_initialized| will get invoked in-line in this function if |*this| is
  // already ready for use, and asynchronously after this function returns
  // otherwise.
  void SetOnInitialized(const base::Closure& on_initialized);

  // Called on RenderThread to create a hardware accelerated video decoder
  // in the GPU process.
  //
  // A routing ID for the GLES2 context needs to be provided when creating a
  // hardware video decoder. This is important because the resources used by
  // the video decoder needs to be shared with the GLES2 context corresponding
  // to the RenderView.
  //
  // This means that a GPU video decoder is tied to a specific RenderView and
  // its GLES2 context in the GPU process.
  //
  // Returns a GpuVideoDecoderHost as a handle to control the video decoder.
  //
  // Note: OnFilterAdded() MUST be called before these methods are called,
  // because they require |channel_| to be non-NULL.
  GpuVideoDecoderHost* CreateVideoDecoder(int context_route_id);

  GpuVideoDecodeAcceleratorHost* CreateVideoAccelerator(
      media::VideoDecodeAccelerator::Client* client);

 private:
  // Guards all members other than |router_|.
  base::Lock lock_;

  // Reference to the channel that the service listens to.
  IPC::Channel* channel_;

  // Router to send messages to a GpuVideoDecoderHost.
  MessageRouter router_;

  // ID for the next GpuVideoDecoderHost.
  int32 next_decoder_host_id_;

  // Callback to invoke when initialized.
  base::Closure on_initialized_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoServiceHost);
};

#endif  // CONTENT_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
