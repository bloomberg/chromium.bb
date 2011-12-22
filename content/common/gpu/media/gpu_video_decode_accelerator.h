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
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Initialize the accelerator with the given profile and send the
  // |init_done_msg| when done.
  void Initialize(const media::VideoDecodeAccelerator::Profile profile,
                  IPC::Message* init_done_msg);

 private:

  // Handlers for IPC messages.
  void OnDecode(base::SharedMemoryHandle handle, int32 id, int32 size);
  void OnAssignPictureBuffers(
      const std::vector<int32>& buffer_ids,
      const std::vector<uint32>& texture_ids,
      const std::vector<gfx::Size>& sizes);
  void OnReusePictureBuffer(
      int32 picture_buffer_id);
  void OnFlush();
  void OnReset();
  void OnDestroy();

  // Pointer to the IPC message sender.
  IPC::Message::Sender* sender_;

  // Message to Send() when initialization is done.  Is only non-NULL during
  // initialization and is owned by the IPC channel underlying the
  // GpuCommandBufferStub.
  IPC::Message* init_done_msg_;

  // Route ID to communicate with the host.
  int32 host_route_id_;

  // Unowned pointer to the underlying GpuCommandBufferStub.
  GpuCommandBufferStub* stub_;

  // Pointer to the underlying VideoDecodeAccelerator.
  scoped_refptr<media::VideoDecodeAccelerator> video_decode_accelerator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
