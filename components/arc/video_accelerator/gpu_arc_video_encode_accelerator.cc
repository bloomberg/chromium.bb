// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"

#include <utility>

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "media/base/video_types.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

#define DVLOGF(x) DVLOG(x) << __func__ << "(): "

namespace arc {

namespace {

void DropSharedMemory(std::unique_ptr<base::SharedMemory> shm) {
  // Just let |shm| fall out of scope.
}

}  // namespace

GpuArcVideoEncodeAccelerator::GpuArcVideoEncodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences) {}

GpuArcVideoEncodeAccelerator::~GpuArcVideoEncodeAccelerator() = default;

// VideoEncodeAccelerator::Client implementation.
void GpuArcVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& coded_size,
    size_t output_buffer_size) {
  DVLOGF(2) << "input_count=" << input_count
            << ", coded_size=" << coded_size.ToString()
            << ", output_buffer_size=" << output_buffer_size;
  DCHECK(client_);
  coded_size_ = coded_size;
  client_->RequireBitstreamBuffers(input_count, coded_size, output_buffer_size);
}

void GpuArcVideoEncodeAccelerator::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOGF(2) << "id=" << bitstream_buffer_id;
  DCHECK(client_);
  auto iter = use_bitstream_cbs_.find(bitstream_buffer_id);
  DCHECK(iter != use_bitstream_cbs_.end());
  std::move(iter->second)
      .Run(metadata.payload_size_bytes, metadata.key_frame,
           metadata.timestamp.InMicroseconds());
  use_bitstream_cbs_.erase(iter);
}

void GpuArcVideoEncodeAccelerator::NotifyError(Error error) {
  DVLOGF(2) << "error=" << error;
  DCHECK(client_);
  client_->NotifyError(error);
}

// ::arc::mojom::VideoEncodeAccelerator implementation.
void GpuArcVideoEncodeAccelerator::GetSupportedProfiles(
    GetSupportedProfilesCallback callback) {
  std::move(callback).Run(
      media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          gpu_preferences_));
}

void GpuArcVideoEncodeAccelerator::Initialize(
    const media::VideoEncodeAccelerator::Config& config,
    VideoEncodeClientPtr client,
    InitializeCallback callback) {
  DVLOGF(2) << config.AsHumanReadableString();
  if (!config.storage_type.has_value()) {
    DLOG(ERROR) << "storage type must be specified";
    std::move(callback).Run(false);
    return;
  }
  input_pixel_format_ = config.input_format;
  input_storage_type_ = *config.storage_type;
  visible_size_ = config.input_visible_size;
  accelerator_ = media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
      config, this, gpu_preferences_);
  if (accelerator_ == nullptr) {
    DLOG(ERROR) << "Failed to create a VideoEncodeAccelerator.";
    std::move(callback).Run(false);
    return;
  }
  client_ = std::move(client);
  std::move(callback).Run(true);
}

void GpuArcVideoEncodeAccelerator::EncodeDeprecated(
    mojo::ScopedHandle handle,
    std::vector<::arc::VideoFramePlane> planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  Encode(input_pixel_format_, std::move(handle), planes, timestamp,
         force_keyframe, std::move(callback));
}

void GpuArcVideoEncodeAccelerator::Encode(
    media::VideoPixelFormat format,
    mojo::ScopedHandle handle,
    std::vector<::arc::VideoFramePlane> planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  DVLOGF(2) << "timestamp=" << timestamp;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  if (planes.empty()) {  // EOS
    accelerator_->Encode(media::VideoFrame::CreateEOSFrame(), force_keyframe);
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  if (input_storage_type_ ==
      media::VideoEncodeAccelerator::Config::StorageType::kShmem) {
    EncodeSharedMemory(std::move(fd), format, planes, timestamp, force_keyframe,
                       std::move(callback));
  } else {
    EncodeDmabuf(std::move(fd), format, planes, timestamp, force_keyframe,
                 std::move(callback));
  }
}

void GpuArcVideoEncodeAccelerator::EncodeDmabuf(
    base::ScopedFD fd,
    media::VideoPixelFormat format,
    const std::vector<::arc::VideoFramePlane>& planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  client_->NotifyError(Error::kInvalidArgumentError);
  NOTIMPLEMENTED();
}

void GpuArcVideoEncodeAccelerator::EncodeSharedMemory(
    base::ScopedFD fd,
    media::VideoPixelFormat format,
    const std::vector<::arc::VideoFramePlane>& planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  if (format != media::PIXEL_FORMAT_I420) {
    DLOG(ERROR) << "Formats other than I420 are unsupported. format=" << format;
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  size_t allocation_size =
      media::VideoFrame::AllocationSize(format, coded_size_);
  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::SharedMemoryHandle shm_handle(base::FileDescriptor(fd.release(), true),
                                      0u, guid);
  auto shm = std::make_unique<base::SharedMemory>(shm_handle, true);

  base::CheckedNumeric<off_t> map_offset = planes[0].offset;
  base::CheckedNumeric<size_t> map_size = allocation_size;
  const uint32_t aligned_offset =
      planes[0].offset % base::SysInfo::VMAllocationGranularity();
  map_offset -= aligned_offset;
  map_size += aligned_offset;

  if (!map_offset.IsValid() || !map_size.IsValid()) {
    DLOG(ERROR) << "Invalid map_offset or map_size";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  if (!shm->MapAt(map_offset.ValueOrDie(), map_size.ValueOrDie())) {
    DLOG(ERROR) << "Failed to map memory.";
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  uint8_t* shm_memory = reinterpret_cast<uint8_t*>(shm->memory());
  auto frame = media::VideoFrame::WrapExternalSharedMemory(
      format, coded_size_, gfx::Rect(visible_size_), visible_size_,
      shm_memory + aligned_offset, allocation_size, shm_handle,
      planes[0].offset, base::TimeDelta::FromMicroseconds(timestamp));

  // Add the function to relase |shm| and |callback| to |frame|'s  destruction
  // observer. When the |frame| goes out of scope, it unmaps and releases the
  // shared memory as well as executes |callback|.
  frame->AddDestructionObserver(
      base::BindOnce(&DropSharedMemory, std::move(shm)));
  frame->AddDestructionObserver(std::move(callback));
  accelerator_->Encode(frame, force_keyframe);
}

void GpuArcVideoEncodeAccelerator::UseBitstreamBuffer(
    mojo::ScopedHandle shmem_fd,
    uint32_t offset,
    uint32_t size,
    UseBitstreamBufferCallback callback) {
  DVLOGF(2) << "serial=" << bitstream_buffer_serial_;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(shmem_fd));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::SharedMemoryHandle shm_handle(base::FileDescriptor(fd.release(), true),
                                      0u, guid);
  use_bitstream_cbs_.emplace(bitstream_buffer_serial_, std::move(callback));
  accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_serial_, shm_handle, size, offset));

  // Mask against 30 bits to avoid (undefined) wraparound on signed integer.
  bitstream_buffer_serial_ = (bitstream_buffer_serial_ + 1) & 0x3FFFFFFF;
}

void GpuArcVideoEncodeAccelerator::RequestEncodingParametersChange(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->RequestEncodingParametersChange(bitrate, framerate);
}

void GpuArcVideoEncodeAccelerator::Flush(FlushCallback callback) {
  DVLOGF(2);
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->Flush(std::move(callback));
}

}  // namespace arc
