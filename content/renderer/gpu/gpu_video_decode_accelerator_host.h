// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel.h"
#include "media/video/video_decode_accelerator.h"

class GpuChannelHost;

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Channel::Listener,
      public media::VideoDecodeAccelerator,
      public base::NonThreadSafe,
      public base::SupportsWeakPtr<GpuVideoDecodeAcceleratorHost> {
 public:
  // |channel| is used to send IPC messages to GPU process.
  GpuVideoDecodeAcceleratorHost(GpuChannelHost* channel,
                                int32 decoder_route_id,
                                media::VideoDecodeAccelerator::Client* client);
  virtual ~GpuVideoDecodeAcceleratorHost();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(Profile profile) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignPictureBuffers(
      const std::vector<media::PictureBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;
  virtual void Destroy() OVERRIDE;

 private:
  void Send(IPC::Message* message);

  void OnBitstreamBufferProcessed(int32 bitstream_buffer_id);
  void OnProvidePictureBuffer(
    uint32 num_requested_buffers, const gfx::Size& buffer_size);
  void OnDismissPictureBuffer(int32 picture_buffer_id);
  void OnPictureReady(int32 picture_buffer_id, int32 bitstream_buffer_id);
  void OnFlushDone();
  void OnResetDone();
  void OnEndOfStream();
  void OnErrorNotification(uint32 error);

  // Sends IPC messages to the Gpu process.
  GpuChannelHost* channel_;

  // Route ID for the associated decoder in the GPU process.
  // TODO(fischman): storing route_id's for GPU process entities in the renderer
  // process is vulnerable to GPU process crashing & being respawned, and
  // attempting to use an outdated or reused route id.
  int32 decoder_route_id_;

  // Reference to the client that will receive callbacks from the decoder.
  media::VideoDecodeAccelerator::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

#endif  // CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
