// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message.h"
#include "media/video/video_decode_accelerator.h"

class GpuVideoDecodeAccelerator
    : public base::RefCountedThreadSafe<GpuVideoDecodeAccelerator>,
      public IPC::Channel::Listener,
      public IPC::Message::Sender,
      public media::VideoDecodeAccelerator::Client {
 public:
  GpuVideoDecodeAccelerator(IPC::Message::Sender* sender, int32 host_route_id);
  virtual ~GpuVideoDecodeAccelerator();

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(
      uint32 requested_num_of_buffers,
      const gfx::Size& dimensions,
      media::VideoDecodeAccelerator::MemoryType type) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfStream() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyAbortDone() OVERRIDE;

  // Function to delegate sending to actual sender.
  virtual bool Send(IPC::Message* message);

  void set_video_decode_accelerator(
      media::VideoDecodeAccelerator* accelerator) {
    video_decode_accelerator_ = accelerator;
  }

 private:
  // Handlers for IPC messages.
  void OnGetConfigs(const std::vector<uint32>& config,
                    std::vector<uint32>* configs);
  void OnInitialize(const std::vector<uint32>& configs);
  void OnDecode(int32 id, base::SharedMemoryHandle handle, int32 size);
  void OnAssignGLESBuffers(const std::vector<int32> buffer_ids,
                           const std::vector<uint32> texture_ids,
                           const std::vector<uint32> context_ids,
                           const std::vector<gfx::Size> sizes);
  void OnAssignSysmemBuffers(const std::vector<int32> buffer_ids,
                             const std::vector<base::SharedMemoryHandle> data,
                             const std::vector<gfx::Size> sizes);
  void OnReusePictureBuffer(int32 picture_buffer_id);
  void OnFlush();
  void OnAbort();

  // Pointer to the IPC message sender.
  IPC::Message::Sender* sender_;

  // Route ID to communicate with the host.
  int32 route_id_;

  // Pointer to the underlying VideoDecodeAccelerator.
  media::VideoDecodeAccelerator* video_decode_accelerator_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

#endif  // CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_
