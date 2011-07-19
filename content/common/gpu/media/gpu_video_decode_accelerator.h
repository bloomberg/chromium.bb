// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "media/video/video_decode_accelerator.h"

namespace gpu {
class ReadWriteTokens;
}

class GpuCommandBufferStub;

class GpuVideoDecodeAccelerator
    : public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public media::VideoDecodeAccelerator::Client {
 public:
  GpuVideoDecodeAccelerator(IPC::Message::Sender* sender,
                            int32 host_route_id,
                            GpuCommandBufferStub* stub);
  virtual ~GpuVideoDecodeAccelerator();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers, const gfx::Size& dimensions) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;

  // Function to delegate sending to actual sender.
  virtual bool Send(IPC::Message* message);

  // Callback to be fired when the underlying stub receives a new token.
  void OnSetToken(int32 token);

  // Initialize the accelerator with the given configuration.
  void Initialize(const std::vector<uint32>& configs);

 private:
  // Defers |msg| for later processing if it specifies a write token that hasn't
  // come to pass yet, and set |*deferred| to true.  Return false if the message
  // failed to parse.
  bool DeferMessageIfNeeded(const IPC::Message& msg, bool* deferred);

  // Handlers for IPC messages.
  void OnDecode(
      const gpu::ReadWriteTokens& /* tokens */,
      base::SharedMemoryHandle handle, int32 id, int32 size);
  void OnAssignPictureBuffers(
      const gpu::ReadWriteTokens& /* tokens */,
      const std::vector<int32>& buffer_ids,
      const std::vector<uint32>& texture_ids,
      const std::vector<gfx::Size>& sizes);
  void OnReusePictureBuffer(
      const gpu::ReadWriteTokens& /* tokens */,
      int32 picture_buffer_id);
  void OnFlush(const gpu::ReadWriteTokens& /* tokens */);
  void OnReset(const gpu::ReadWriteTokens& /* tokens */);
  void OnDestroy(const gpu::ReadWriteTokens& /* tokens */);

  // Pointer to the IPC message sender.
  IPC::Message::Sender* sender_;

  // Route ID to communicate with the host.
  int32 host_route_id_;

  // Messages deferred for later processing when their tokens have come to pass.
  std::vector<IPC::Message*> deferred_messages_;

  // Unowned pointer to the underlying GpuCommandBufferStub.
  GpuCommandBufferStub* stub_;

  // Pointer to the underlying VideoDecodeAccelerator.
  scoped_refptr<media::VideoDecodeAccelerator> video_decode_accelerator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
