// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_RENDERER_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ipc/ipc_channel.h"
#include "media/video/video_decode_accelerator.h"

class MessageLoop;
class MessageRouter;

// This class is used to talk to VideoDecodeAccelerator in the Gpu process
// through IPC messages.
class GpuVideoDecodeAcceleratorHost : public IPC::Channel::Listener,
                                      public media::VideoDecodeAccelerator {
 public:
  // |router| is used to dispatch IPC messages to this object.
  // |ipc_sender| is used to send IPC messages to Gpu process.
  GpuVideoDecodeAcceleratorHost(MessageRouter* router,
                                IPC::Message::Sender* ipc_sender,
                                int32 decoder_host_id,
                                media::VideoDecodeAccelerator::Client* client);
  virtual ~GpuVideoDecodeAcceleratorHost();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual void GetConfigs(
      const std::vector<uint32>& requested_configs,
      std::vector<uint32>* matched_configs) OVERRIDE;
  virtual bool Initialize(const std::vector<uint32>& configs) OVERRIDE;
  virtual bool Decode(const media::BitstreamBuffer& bitstream_buffer) OVERRIDE;
  virtual void AssignGLESBuffers(
      const std::vector<media::GLESBuffer>& buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(
      const std::vector<media::SysmemBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual bool Flush() OVERRIDE;
  virtual bool Abort() OVERRIDE;

 private:
  void OnBitstreamBufferProcessed(int32 bitstream_buffer_id);
  void OnProvidePictureBuffer(
    uint32 num_requested_buffers, const gfx::Size& buffer_size, int32 mem_type);
  void OnDismissPictureBuffer(int32 picture_buffer_id);
  void OnCreateDone(int32 decoder_id);
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

  // Temporarily store configs here in between Create and Initialize phase.
  std::vector<uint32> configs_;

  // Reference to the client that will receive callbacks from the decoder.
  media::VideoDecodeAccelerator::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(GpuVideoDecodeAcceleratorHost);
};

#endif  // CONTENT_RENDERER_GPU_VIDEO_DECODE_ACCELERATOR_HOST_H_
