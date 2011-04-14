// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <deque>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_callback_factory.h"
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
      const std::vector<uint32>& buffer_properties) OVERRIDE;
  virtual void DismissPictureBuffer(
      media::VideoDecodeAccelerator::PictureBuffer* picture_buffer) OVERRIDE;
  virtual void PictureReady(
      media::VideoDecodeAccelerator::Picture* picture) OVERRIDE;
  virtual void NotifyEndOfStream();
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error);

  // Function to delegate sending to actual sender.
  virtual bool Send(IPC::Message* message);

  void set_video_decode_accelerator(
      media::VideoDecodeAccelerator* accelerator) {
    video_decode_accelerator_ = accelerator;
  }

 private:
  // Handlers for IPC messages.
  void OnGetConfigs(std::vector<uint32> config, std::vector<uint32>* configs);
  void OnCreate(std::vector<uint32> config, int32* decoder_id);
  void OnDecode(base::SharedMemoryHandle handle, int32 offset, int32 size);
  void OnAssignPictureBuffer(int32 picture_buffer_id,
                             base::SharedMemoryHandle handle,
                             std::vector<uint32> texture_ids);
  void OnReusePictureBuffer(int32 picture_buffer_id);
  void OnFlush();
  void OnAbort();

  // One-time callbacks from the accelerator.
  void OnBitstreamBufferProcessed();
  void OnFlushDone();
  void OnAbortDone();

  // Pointer to the IPC message sender.
  IPC::Message::Sender* sender_;
  // Route ID to communicate with the host.
  int32 route_id_;
  // Pointer to the underlying VideoDecodeAccelerator.
  media::VideoDecodeAccelerator* video_decode_accelerator_;
  // Callback factory to generate one-time callbacks.
  base::ScopedCallbackFactory<GpuVideoDecodeAccelerator> cb_factory_;
  // Container to hold shared memory blocks so that we can return the
  // information about their consumption to renderer.
  std::deque<base::SharedMemory*> shm_in_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

#endif  // CONTENT_GPU_GPU_VIDEO_DECODE_ACCELERATOR_H_
