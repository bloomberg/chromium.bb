// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_video_decode_accelerator.h"

#include <vector>

#include "base/shared_memory.h"
#include "content/common/gpu_messages.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "media/video/picture.h"
#include "ui/gfx/size.h"

GpuVideoDecodeAccelerator::GpuVideoDecodeAccelerator(
    IPC::Message::Sender* sender,
    int32 host_route_id)
    : sender_(sender),
      route_id_(host_route_id),
      cb_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

GpuVideoDecodeAccelerator::~GpuVideoDecodeAccelerator() {}

bool GpuVideoDecodeAccelerator::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuVideoDecodeAccelerator, msg)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_GetConfigs,
                        OnGetConfigs)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Decode,
                        OnDecode)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_AssignPictureBuffer,
                        OnAssignPictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_ReusePictureBuffer,
                        OnReusePictureBuffer)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Flush,
                        OnFlush)
    IPC_MESSAGE_HANDLER(AcceleratedVideoDecoderMsg_Abort,
                        OnAbort)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GpuVideoDecodeAccelerator::OnChannelConnected(int32 peer_pid) {
  // TODO(vmr): Do we have to react on channel connections?
}

void GpuVideoDecodeAccelerator::OnChannelError() {
  // TODO(vmr): Do we have to react on channel errors?
}

void GpuVideoDecodeAccelerator::ProvidePictureBuffers(
    uint32 requested_num_of_buffers,
    const std::vector<uint32>& buffer_properties) {
  Send(new AcceleratedVideoDecoderHostMsg_ProvidePictureBuffers(
           route_id_,
           requested_num_of_buffers,
           buffer_properties));
}

void GpuVideoDecodeAccelerator::DismissPictureBuffer(
    media::VideoDecodeAccelerator::PictureBuffer* picture_buffer) {
  DCHECK(picture_buffer);
  // TODO(vmr): Unmap system memory here.
  Send(new AcceleratedVideoDecoderHostMsg_DismissPictureBuffer(
           route_id_,
           picture_buffer->GetId()));
}

void GpuVideoDecodeAccelerator::PictureReady(
    media::VideoDecodeAccelerator::Picture* picture) {
  Send(new AcceleratedVideoDecoderHostMsg_PictureReady(
           route_id_,
           picture->picture_buffer()->GetId()));
}

void GpuVideoDecodeAccelerator::NotifyEndOfStream() {
  Send(new AcceleratedVideoDecoderHostMsg_EndOfStream(route_id_));
}

void GpuVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error) {
  Send(new AcceleratedVideoDecoderHostMsg_ErrorNotification(route_id_,
                                                            error));
}

void GpuVideoDecodeAccelerator::OnGetConfigs(std::vector<uint32> config,
                                             std::vector<uint32>* configs) {
  DCHECK(configs);
  *configs = video_decode_accelerator_->GetConfig(config);
}

void GpuVideoDecodeAccelerator::OnCreate(std::vector<uint32> config,
                                         int32* decoder_id) {
}

void GpuVideoDecodeAccelerator::OnDecode(base::SharedMemoryHandle handle,
                                         int32 offset,
                                         int32 size) {
  if (offset < 0 || size <= 0) {
    NotifyError(media::VideoDecodeAccelerator::VIDEODECODERERROR_INVALIDINPUT);
    return;
  }
  if (!base::SharedMemory::IsHandleValid(handle)) {
    NotifyError(media::VideoDecodeAccelerator::VIDEODECODERERROR_MEMFAILURE);
    return;
  }
  base::SharedMemory* shm = new base::SharedMemory(handle, true);
  if (!shm || !shm->Map(offset + size)) {
    NotifyError(media::VideoDecodeAccelerator::VIDEODECODERERROR_MEMFAILURE);
    return;
  }
  // Set the freshly mapped memory address to the bitstream buffer.
  uint8* mem_ptr = static_cast<uint8*>(shm->memory());
  media::BitstreamBuffer bitstream(mem_ptr + offset, size, NULL);
  // Store the SHM in our FIFO queue. We need to do this before Decode because
  // it is legal to call BitstreamBufferProcessed callback from the Decode
  // context.
  shm_in_.push_back(shm);
  video_decode_accelerator_->Decode(
      &bitstream,
      cb_factory_.NewCallback(
          &GpuVideoDecodeAccelerator::OnBitstreamBufferProcessed));
}

void GpuVideoDecodeAccelerator::OnAssignPictureBuffer(
    int32 picture_buffer_id,
    base::SharedMemoryHandle handle,
    std::vector<uint32> textures) {
  // TODO(vmr): Get the right size for picture buffers from config.
  gfx::Size size(320, 240);
  uint32 bits_per_pixel = 32;
  media::VideoDecodeAccelerator::PictureBuffer::MemoryType memory_type;
  std::vector<media::VideoDecodeAccelerator::PictureBuffer::DataPlaneHandle>
      planes;
  if (handle == base::SharedMemory::NULLHandle()) {
    // TODO(vmr): Handle GLES textures here.
    memory_type = media::VideoDecodeAccelerator::PictureBuffer::
        PICTUREBUFFER_MEMORYTYPE_GL_TEXTURE;
  } else {
    // Input buffer provided is in system memory in one plane.
    memory_type = media::VideoDecodeAccelerator::PictureBuffer::
        PICTUREBUFFER_MEMORYTYPE_SYSTEM;
    if (!base::SharedMemory::IsHandleValid(handle)) {
      NotifyError(media::VideoDecodeAccelerator::VIDEODECODERERROR_MEMFAILURE);
      return;
    }
    base::SharedMemory* shm = new base::SharedMemory(handle, true);
    if (!shm || !shm->Map(size.width() * size.height() * bits_per_pixel)) {
      NotifyError(media::VideoDecodeAccelerator::VIDEODECODERERROR_MEMFAILURE);
      return;
    }
    media::VideoDecodeAccelerator::PictureBuffer::DataPlaneHandle sysmem_plane;
    sysmem_plane.sysmem = shm;
    planes.push_back(sysmem_plane);
  }
  std::vector<media::VideoDecodeAccelerator::PictureBuffer*> bufs;
  std::vector<uint32> color_format;
  media::VideoDecodeAccelerator::PictureBuffer* picture_buffer =
      new media::PictureBuffer(picture_buffer_id,
                               size,
                               color_format,
                               memory_type,
                               planes);
  bufs.push_back(picture_buffer);
  video_decode_accelerator_->AssignPictureBuffer(bufs);
}

void GpuVideoDecodeAccelerator::OnReusePictureBuffer(
    int32 picture_buffer_id) {
  // TODO(vmr): Get the picture buffer with the id.
  media::VideoDecodeAccelerator::PictureBuffer* picture_buffer = NULL;
  video_decode_accelerator_->ReusePictureBuffer(picture_buffer);
}

void GpuVideoDecodeAccelerator::OnFlush() {
  if (!video_decode_accelerator_->Flush(cb_factory_.NewCallback(
          &GpuVideoDecodeAccelerator::OnFlushDone))) {
    NotifyError(
        media::VideoDecodeAccelerator::VIDEODECODERERROR_UNEXPECTED_FLUSH);
  }
}

void GpuVideoDecodeAccelerator::OnAbort() {
  video_decode_accelerator_->Abort(cb_factory_.NewCallback(
      &GpuVideoDecodeAccelerator::OnAbortDone));
}

void GpuVideoDecodeAccelerator::OnBitstreamBufferProcessed() {
  base::SharedMemory* shm = shm_in_.front();
  DCHECK(shm);  // Shared memory should always be non-NULL.
  Send(new AcceleratedVideoDecoderHostMsg_BitstreamBufferProcessed(
           0, shm->handle()));
  shm_in_.pop_front();
}

void GpuVideoDecodeAccelerator::OnFlushDone() {
  Send(new AcceleratedVideoDecoderHostMsg_FlushDone(0));
}

void GpuVideoDecodeAccelerator::OnAbortDone() {
  Send(new AcceleratedVideoDecoderHostMsg_AbortDone(0));
}

bool GpuVideoDecodeAccelerator::Send(IPC::Message* message) {
  DCHECK(sender_);
  return sender_->Send(message);
}

