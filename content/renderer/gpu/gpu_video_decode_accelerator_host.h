// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/threading/non_thread_safe.h"
#include "ipc/ipc_channel.h"
#include "media/video/video_decode_accelerator.h"

class MessageLoop;
class MessageRouter;
namespace gpu {
class CommandBufferHelper;
class ReadWriteTokens;
}

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost
    : public IPC::Channel::Listener,
      public media::VideoDecodeAccelerator,
      public base::NonThreadSafe {
 public:
  // |router| is used to dispatch IPC messages to this object.
  // |ipc_sender| is used to send IPC messages to Gpu process.
  GpuVideoDecodeAcceleratorHost(MessageRouter* router,
                                IPC::Message::Sender* ipc_sender,
                                int32 decoder_host_id,
                                int32 command_buffer_route_id,
                                gpu::CommandBufferHelper* cmd_buffer_helper,
                                media::VideoDecodeAccelerator::Client* client);
  virtual ~GpuVideoDecodeAcceleratorHost();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual bool GetConfigs(
      const std::vector<uint32>& requested_configs,
      std::vector<uint32>* matched_configs) OVERRIDE;
  virtual bool Initialize(const std::vector<uint32>& configs) OVERRIDE;
  virtual void Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignGLESBuffers(
      const std::vector<media::GLESBuffer>& buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(
      const std::vector<media::SysmemBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Abort() OVERRIDE;

 private:
  // Insert a token into the command buffer and return a token-pair suitable for
  // sending over IPC for synchronization with the command buffer.
  gpu::ReadWriteTokens SyncTokens();

  void OnBitstreamBufferProcessed(int32 bitstream_buffer_id);
  void OnProvidePictureBuffer(
    uint32 num_requested_buffers, const gfx::Size& buffer_size, int32 mem_type);
  void OnDismissPictureBuffer(int32 picture_buffer_id);
  void OnCreateDone(int32 decoder_id);
  void OnInitializeDone();
  void OnPictureReady(int32 picture_buffer_id,
                      int32 bitstream_buffer_id,
                      const gfx::Size& visible_size,
                      const gfx::Size& decoded_size);
  void OnFlushDone();
  void OnAbortDone();
  void OnEndOfStream();
  void OnErrorNotification(uint32 error);

  // A router used to send us IPC messages.
  MessageRouter* router_;

  // Sends IPC messages to the Gpu process.
  IPC::Message::Sender* ipc_sender_;

  // ID of this GpuVideoDecodeAcceleratorHost.
  int32 decoder_host_id_;

  // ID of VideoDecodeAccelerator in the Gpu process.
  int32 decoder_id_;

  // Route ID for the command buffer associated with the context the GPU Video
  // Decoder uses.
  // TODO(fischman): storing route_id's for GPU process entities in the renderer
  // process is vulnerable to GPU process crashing & being respawned, and
  // attempting to use an outdated or reused route id.
  int32 command_buffer_route_id_;

  // Helper for the command buffer associated with the context the GPU Video
  // Decoder uses.
  // TODO(fischman): in the out-of-process case, this won't work
  // (context3d->gles2_impl() will be NULL), and will have to be replaced with a
  // dedicated message such as WaitForToken, which will serialize subsequent
  // message processing behind it.
  gpu::CommandBufferHelper* cmd_buffer_helper_;

  // Temporarily store configs here in between Create and Initialize phase.
  std::vector<uint32> configs_;

  // Reference to the client that will receive callbacks from the decoder.
  media::VideoDecodeAccelerator::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

#endif  // CONTENT_RENDERER_GPU_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
