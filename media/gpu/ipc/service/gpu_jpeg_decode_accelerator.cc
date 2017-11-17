// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/gpu_jpeg_decode_accelerator.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"

namespace {

void DecodeFinished(std::unique_ptr<base::SharedMemory> shm) {
  // Do nothing. Because VideoFrame is backed by |shm|, the purpose of this
  // function is to just keep reference of |shm| to make sure it lives until
  // decode finishes.
}

bool VerifyDecodeParams(const gfx::Size& coded_size,
                        mojo::ScopedSharedBufferHandle* output_handle,
                        uint32_t output_buffer_size) {
  const int kJpegMaxDimension = UINT16_MAX;
  if (coded_size.IsEmpty() || coded_size.width() > kJpegMaxDimension ||
      coded_size.height() > kJpegMaxDimension) {
    LOG(ERROR) << "invalid coded_size " << coded_size.ToString();
    return false;
  }

  if (!output_handle->is_valid()) {
    LOG(ERROR) << "invalid output_handle";
    return false;
  }

  if (output_buffer_size <
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, coded_size)) {
    LOG(ERROR) << "output_buffer_size is too small: " << output_buffer_size;
    return false;
  }

  return true;
}

}  // namespace

namespace media {

// static
void GpuJpegDecodeAccelerator::Create(
    mojom::GpuJpegDecodeAcceleratorRequest request) {
  auto* jpeg_decoder = new GpuJpegDecodeAccelerator();
  mojo::MakeStrongBinding(base::WrapUnique(jpeg_decoder), std::move(request));
}

GpuJpegDecodeAccelerator::GpuJpegDecodeAccelerator()
    : accelerator_factory_functions_(
          GpuJpegDecodeAcceleratorFactoryProvider::GetAcceleratorFactories()) {}

GpuJpegDecodeAccelerator::~GpuJpegDecodeAccelerator() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void GpuJpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(bitstream_buffer_id,
                     JpegDecodeAccelerator::Error::NO_ERRORS);
}

void GpuJpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                           JpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(bitstream_buffer_id, error);
}

void GpuJpegDecodeAccelerator::Initialize(InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegDecoder::Initialize
  // needs to be updated.

  std::unique_ptr<JpegDecodeAccelerator> accelerator;
  for (const auto& create_jda_function : accelerator_factory_functions_) {
    std::unique_ptr<JpegDecodeAccelerator> tmp_accelerator =
        create_jda_function.Run(base::ThreadTaskRunnerHandle::Get());
    if (tmp_accelerator && tmp_accelerator->Initialize(this)) {
      accelerator = std::move(tmp_accelerator);
      break;
    }
  }

  if (!accelerator) {
    DLOG(ERROR) << "JPEG accelerator Initialize failed";
    std::move(callback).Run(false);
    return;
  }

  accelerator_ = std::move(accelerator);
  std::move(callback).Run(true);
}

void GpuJpegDecodeAccelerator::Decode(
    const BitstreamBuffer& input_buffer,
    const gfx::Size& coded_size,
    mojo::ScopedSharedBufferHandle output_handle,
    uint32_t output_buffer_size,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("jpeg", "GpuJpegDecodeAccelerator::Decode");

  DCHECK(!decode_cb_);
  decode_cb_ = std::move(callback);

  if (!VerifyDecodeParams(coded_size, &output_handle, output_buffer_size)) {
    NotifyDecodeStatus(input_buffer.id(),
                       JpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  base::SharedMemoryHandle memory_handle;
  MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(output_handle), &memory_handle, nullptr, nullptr);
  DCHECK_EQ(MOJO_RESULT_OK, result);

  std::unique_ptr<base::SharedMemory> output_shm(
      new base::SharedMemory(memory_handle, false));
  if (!output_shm->Map(output_buffer_size)) {
    LOG(ERROR) << "Could not map output shared memory for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(input_buffer.id(),
                       JpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }

  uint8_t* shm_memory = static_cast<uint8_t*>(output_shm->memory());
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapExternalSharedMemory(
      PIXEL_FORMAT_I420,      // format
      coded_size,             // coded_size
      gfx::Rect(coded_size),  // visible_rect
      coded_size,             // natural_size
      shm_memory,             // data
      output_buffer_size,     // data_size
      memory_handle,          // handle
      0,                      // data_offset
      base::TimeDelta());     // timestamp
  if (!frame.get()) {
    LOG(ERROR) << "Could not create VideoFrame for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(input_buffer.id(),
                       JpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }
  frame->AddDestructionObserver(
      base::Bind(DecodeFinished, base::Passed(&output_shm)));

  DCHECK(accelerator_);
  accelerator_->Decode(input_buffer, frame);
}

void GpuJpegDecodeAccelerator::Uninitialize() {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

void GpuJpegDecodeAccelerator::NotifyDecodeStatus(
    int32_t bitstream_buffer_id,
    JpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(decode_cb_);
  std::move(decode_cb_).Run(bitstream_buffer_id, error);
}

}  // namespace media
