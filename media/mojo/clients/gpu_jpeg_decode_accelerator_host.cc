// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/gpu_jpeg_decode_accelerator_host.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_handle.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

GpuJpegDecodeAcceleratorHost::GpuJpegDecodeAcceleratorHost(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    mojom::GpuJpegDecodeAcceleratorPtrInfo jpeg_decoder)
    : io_task_runner_(std::move(io_task_runner)),
      jpeg_decoder_info_(std::move(jpeg_decoder)) {}

GpuJpegDecodeAcceleratorHost::~GpuJpegDecodeAcceleratorHost() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

bool GpuJpegDecodeAcceleratorHost::Initialize(
    JpegDecodeAccelerator::Client* /*client*/) {
  NOTIMPLEMENTED();
  return false;
}

void GpuJpegDecodeAcceleratorHost::InitializeAsync(Client* client,
                                                   InitCB init_cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  jpeg_decoder_.Bind(std::move(jpeg_decoder_info_));

  // base::Unretained is safe because |this| owns |jpeg_decoder_|.
  jpeg_decoder_.set_connection_error_handler(
      base::Bind(&GpuJpegDecodeAcceleratorHost::OnLostConnectionToJpegDecoder,
                 base::Unretained(this)));
  jpeg_decoder_->Initialize(
      base::Bind(&GpuJpegDecodeAcceleratorHost::OnInitializeDone,
                 base::Unretained(this), std::move(init_cb), client));
}

void GpuJpegDecodeAcceleratorHost::Decode(
    const BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(jpeg_decoder_.is_bound());

  DCHECK(
      base::SharedMemory::IsHandleValid(video_frame->shared_memory_handle()));

  base::SharedMemoryHandle output_handle =
      base::SharedMemory::DuplicateHandle(video_frame->shared_memory_handle());
  if (!base::SharedMemory::IsHandleValid(output_handle)) {
    DLOG(ERROR) << "Failed to duplicate handle of VideoFrame";
    return;
  }

  size_t output_buffer_size = VideoFrame::AllocationSize(
      video_frame->format(), video_frame->coded_size());
  mojo::ScopedSharedBufferHandle output_frame_handle =
      mojo::WrapSharedMemoryHandle(output_handle, output_buffer_size,
                                   false /* read_only */);

  // base::Unretained is safe because |this| owns |jpeg_decoder_|.
  jpeg_decoder_->Decode(bitstream_buffer, video_frame->coded_size(),
                        std::move(output_frame_handle),
                        base::checked_cast<uint32_t>(output_buffer_size),
                        base::Bind(&GpuJpegDecodeAcceleratorHost::OnDecodeAck,
                                   base::Unretained(this)));
}

bool GpuJpegDecodeAcceleratorHost::IsSupported() {
  return true;
}

void GpuJpegDecodeAcceleratorHost::OnInitializeDone(
    InitCB init_cb,
    JpegDecodeAccelerator::Client* client,
    bool success) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (success)
    client_ = client;

  std::move(init_cb).Run(success);
}

void GpuJpegDecodeAcceleratorHost::OnDecodeAck(
    int32_t bitstream_buffer_id,
    JpegDecodeAccelerator::Error error) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (!client_)
    return;

  if (error == JpegDecodeAccelerator::Error::NO_ERRORS) {
    client_->VideoFrameReady(bitstream_buffer_id);
    return;
  }

  // Only NotifyError once.
  // Client::NotifyError() may trigger deletion of |this|, so calling it needs
  // to be the last thing done on this stack!
  Client* client = nullptr;
  std::swap(client, client_);
  client->NotifyError(bitstream_buffer_id, error);
}

void GpuJpegDecodeAcceleratorHost::OnLostConnectionToJpegDecoder() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  OnDecodeAck(kInvalidBitstreamBufferId,
              JpegDecodeAccelerator::Error::PLATFORM_FAILURE);
}

}  // namespace media
