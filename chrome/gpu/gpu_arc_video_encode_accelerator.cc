// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_arc_video_encode_accelerator.h"

#include <utility>

#include "base/logging.h"
#include "base/sys_info.h"
#include "media/base/video_types.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

#define DVLOGF(x) DVLOG(x) << __func__ << "(): "

namespace chromeos {
namespace arc {

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
    size_t payload_size,
    bool key_frame,
    base::TimeDelta timestamp) {
  DVLOGF(2) << "id=" << bitstream_buffer_id;
  DCHECK(client_);
  client_->BitstreamBufferReady(bitstream_buffer_id, payload_size, key_frame,
                                timestamp.InMicroseconds());
}

void GpuArcVideoEncodeAccelerator::NotifyError(Error error) {
  DVLOGF(2) << "error=" << error;
  DCHECK(client_);
  client_->NotifyError(error);
}

// ::arc::mojom::VideoEncodeAccelerator implementation.
void GpuArcVideoEncodeAccelerator::GetSupportedProfiles(
    const GetSupportedProfilesCallback& callback) {
  callback.Run(media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
      gpu_preferences_));
}

void GpuArcVideoEncodeAccelerator::Initialize(
    VideoPixelFormat input_format,
    const gfx::Size& visible_size,
    VideoEncodeAccelerator::StorageType input_storage,
    VideoCodecProfile output_profile,
    uint32_t initial_bitrate,
    VideoEncodeClientPtr client,
    const InitializeCallback& callback) {
  DVLOGF(2) << "visible_size=" << visible_size.ToString()
            << ", profile=" << output_profile;

  input_pixel_format_ = input_format;
  visible_size_ = visible_size;
  accelerator_ = media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
      input_pixel_format_, visible_size_, output_profile, initial_bitrate, this,
      gpu_preferences_);
  if (accelerator_ == nullptr) {
    DLOG(ERROR) << "Failed to create a VideoEncodeAccelerator.";
    callback.Run(false);
    return;
  }
  client_ = std::move(client);
  callback.Run(true);
}

static void DropSharedMemory(std::unique_ptr<base::SharedMemory> shm) {
  // Just let |shm| fall out of scope.
}

void GpuArcVideoEncodeAccelerator::Encode(
    mojo::ScopedHandle handle,
    std::vector<::arc::VideoFramePlane> planes,
    int64_t timestamp,
    bool force_keyframe) {
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
  if (!fd.is_valid())
    return;

  size_t allocation_size =
      media::VideoFrame::AllocationSize(input_pixel_format_, coded_size_);

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::SharedMemoryHandle shm_handle(base::FileDescriptor(fd.release(), true),
                                      0u, guid);
  auto shm = base::MakeUnique<base::SharedMemory>(shm_handle, true);

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
      input_pixel_format_, coded_size_, gfx::Rect(visible_size_), visible_size_,
      shm_memory + aligned_offset, allocation_size, shm_handle,
      planes[0].offset, base::TimeDelta::FromMicroseconds(timestamp));

  // Wrap |shm| in a callback and add it as a destruction observer, so it
  // stays alive and mapped until |frame| goes out of scope.
  frame->AddDestructionObserver(
      base::Bind(&DropSharedMemory, base::Passed(&shm)));
  accelerator_->Encode(frame, force_keyframe);
}

void GpuArcVideoEncodeAccelerator::UseOutputBitstreamBuffer(
    int32_t bitstream_buffer_id,
    mojo::ScopedHandle shmem_fd,
    uint32_t offset,
    uint32_t size) {
  DVLOGF(2) << "id=" << bitstream_buffer_id;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(shmem_fd));
  if (!fd.is_valid())
    return;

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  base::SharedMemoryHandle shm_handle(base::FileDescriptor(fd.release(), true),
                                      0u, guid);
  accelerator_->UseOutputBitstreamBuffer(
      media::BitstreamBuffer(bitstream_buffer_id, shm_handle, size, offset));
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

base::ScopedFD GpuArcVideoEncodeAccelerator::UnwrapFdFromMojoHandle(
    mojo::ScopedHandle handle) {
  DCHECK(client_);
  if (!handle.is_valid()) {
    DLOG(ERROR) << "handle is invalid.";
    client_->NotifyError(Error::kInvalidArgumentError);
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    DLOG(ERROR) << "UnwrapPlatformFile failed: " << mojo_result;
    client_->NotifyError(Error::kPlatformFailureError);
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

}  // namespace arc
}  // namespace chromeos
