// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
#define CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_

#include "base/singleton.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/gpu_video_decoder_host.h"
#include "ipc/ipc_channel.h"
#include "media/base/buffers.h"
#include "media/base/video_frame.h"

// GpuVideoServiceHost lives on IO thread and is used to dispatch IPC messages
// to GpuVideoDecoderHost objects.
class GpuVideoServiceHost : public IPC::ChannelProxy::MessageFilter {
 public:
  GpuVideoServiceHost();

  // IPC::ChannelProxy::MessageFilter implementations.
  virtual bool OnMessageReceived(const IPC::Message& message);
  virtual void OnFilterAdded(IPC::Channel* channel);
  virtual void OnFilterRemoved();
  virtual void OnChannelClosing();

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
  GpuVideoDecoderHost* CreateVideoDecoder(int context_route_id);

 private:
  IPC::Channel* channel_;

  // Router to send messages to a GpuVideoDecoderHost.
  MessageRouter router_;

  // ID for the next GpuVideoDecoderHost.
  int32 next_decoder_host_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoServiceHost);
};

#endif  // CHROME_RENDERER_GPU_VIDEO_SERVICE_HOST_H_
