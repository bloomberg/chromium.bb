// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
#define CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/shared_memory.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "media/video/video_decode_accelerator.h"

namespace content {

class GpuVideoDecodeAccelerator
    : public IPC::Listener,
      public IPC::Sender,
      public media::VideoDecodeAccelerator::Client,
      public GpuCommandBufferStub::DestructionObserver {
 public:
  // Each of the arguments to the constructor must outlive this object.
  // |stub->decoder()| will be made current around any operation that touches
  // the underlying VDA so that it can make GL calls safely.
  GpuVideoDecodeAccelerator(IPC::Sender* sender,
                            int32 host_route_id,
                            GpuCommandBufferStub* stub);
  virtual ~GpuVideoDecodeAccelerator();

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // media::VideoDecodeAccelerator::Client implementation.
  virtual void ProvidePictureBuffers(uint32 requested_num_of_buffers,
                                     const gfx::Size& dimensions,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 picture_buffer_id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 bitstream_buffer_id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;

  // GpuCommandBufferStub::DestructionObserver implementation.
  virtual void OnWillDestroyStub(GpuCommandBufferStub* stub) OVERRIDE;

  // Function to delegate sending to actual sender.
  virtual bool Send(IPC::Message* message) OVERRIDE;

  // Initialize the accelerator with the given profile and send the
  // |init_done_msg| when done.
  // The renderer process handle is valid as long as we have a channel between
  // GPU process and the renderer.
  void Initialize(const media::VideoCodecProfile profile,
                  IPC::Message* init_done_msg);

 private:
  // Handlers for IPC messages.
  void OnDecode(base::SharedMemoryHandle handle, int32 id, uint32 size);
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
  IPC::Sender* sender_;

  // Message to Send() when initialization is done.  Is only non-NULL during
  // initialization and is owned by the IPC channel underlying the
  // GpuCommandBufferStub.
  IPC::Message* init_done_msg_;

  // Route ID to communicate with the host.
  int32 host_route_id_;

  // Unowned pointer to the underlying GpuCommandBufferStub.
  base::WeakPtr<GpuCommandBufferStub> stub_;

  // The underlying VideoDecodeAccelerator.
  scoped_ptr<media::VideoDecodeAccelerator> video_decode_accelerator_;

  // Callback for making the relevant context current for GL calls.
  // Returns false if failed.
  base::Callback<bool(void)> make_context_current_;

  // The texture target as requested by ProvidePictureBuffers().
  uint32 texture_target_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GpuVideoDecodeAccelerator);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_GPU_VIDEO_DECODE_ACCELERATOR_H_
