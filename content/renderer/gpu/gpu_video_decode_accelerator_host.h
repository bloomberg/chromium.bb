// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel.h"
#include "media/video/video_decode_accelerator.h"

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Channel::Listener,
      public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  // |router| is used to dispatch IPC messages to this object.
  // |ipc_sender| is used to send IPC messages to Gpu process.
  GpuVideoDecodeAcceleratorHost(IPC::Message::Sender* ipc_sender,
                                int32 command_buffer_route_id,
                                media::VideoDecodeAccelerator::Client* client);
  virtual ~GpuVideoDecodeAcceleratorHost();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual bool Initialize(const std::vector<uint32>& configs) OVERRIDE;
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
  void OnInitializeDone();
  void OnPictureReady(int32 picture_buffer_id,
                      int32 bitstream_buffer_id);
  void OnFlushDone();
  void OnResetDone();
  void OnEndOfStream();
  void OnErrorNotification(uint32 error);

  // Sends IPC messages to the Gpu process.
  IPC::Message::Sender* ipc_sender_;

  // Route ID for the command buffer associated with the context the GPU Video
  // Decoder uses.
  // TODO(fischman): storing route_id's for GPU process entities in the renderer
  // process is vulnerable to GPU process crashing & being respawned, and
  // attempting to use an outdated or reused route id.
  int32 command_buffer_route_id_;

  // Reference to the client that will receive callbacks from the decoder.
  media::VideoDecodeAccelerator::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

#endif  // CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
