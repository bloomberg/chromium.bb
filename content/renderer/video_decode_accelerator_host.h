// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_VIDEO_DECODE_ACCELERATOR_HOST_H_
#define CONTENT_RENDERER_VIDEO_DECODE_ACCELERATOR_HOST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ipc/ipc_channel.h"
#include "media/video/video_decode_accelerator.h"

class MessageLoop;
class MessageRouter;

// This class is used to talk to VideoDecodeAccelerator in the GPU process
// through IPC messages.
class VideoDecodeAcceleratorHost : public IPC::Channel::Listener,
                                   public media::VideoDecodeAccelerator {
 public:
  // |router| is used to dispatch IPC messages to this object.
  // |ipc_sender| is used to send IPC messages to GPU process.
  VideoDecodeAcceleratorHost(MessageRouter* router,
                             IPC::Message::Sender* ipc_sender,
                             int32 decoder_host_id);
  virtual ~VideoDecodeAcceleratorHost();

  // IPC::Channel::Listener implementation.
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator implementation.
  virtual void GetConfigs(
      const std::vector<uint32>& requested_configs,
      std::vector<uint32>* matched_configs) OVERRIDE;
  virtual bool Initialize(
      const std::vector<uint32>& config) OVERRIDE;
  virtual bool Decode(
      const media::BitstreamBuffer& bitstream_buffer,
      media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;
  virtual void AssignGLESBuffers(
      const std::vector<media::GLESBuffer>& buffers) OVERRIDE;
  virtual void AssignSysmemBuffers(
      const std::vector<media::SysmemBuffer>& buffers) OVERRIDE;
  virtual void ReusePictureBuffer(uint32 picture_buffer_id) OVERRIDE;
  virtual bool Flush(media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;
  virtual bool Abort(media::VideoDecodeAcceleratorCallback* callback) OVERRIDE;

 private:
  // Message loop that this object runs on.
  MessageLoop* message_loop_;

  // A router used to send us IPC messages.
  MessageRouter* router_;

  // Sends IPC messages to the GPU process.
  IPC::Message::Sender* ipc_sender_;

  // Route ID of the GLES2 context in the GPU process.
  int context_route_id_;

  // ID of this VideoDecodeAcceleratorHost.
  int32 decoder_host_id_;

  // ID of VideoDecodeAccelerator in the GPU process.
  int32 decoder_id_;

  // Transfer buffers for both input and output.
  // TODO(vmr): move into plugin provided IPC buffers.
  scoped_ptr<base::SharedMemory> input_transfer_buffer_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorHost);
};

#endif  // CONTENT_RENDERER_VIDEO_DECODE_ACCELERATOR_HOST_H_
