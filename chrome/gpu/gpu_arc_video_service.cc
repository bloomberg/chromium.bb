// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_arc_video_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/gpu/arc_gpu_video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Make sure arc::mojom::VideoAcceleratorService::Result and
// chromeos::arc::ArcVideoAccelerator::Result match.
static_assert(
    static_cast<int>(arc::mojom::VideoAcceleratorService::Result::SUCCESS) ==
        chromeos::arc::ArcVideoAccelerator::SUCCESS,
    "enum mismatch");
static_assert(static_cast<int>(
                  arc::mojom::VideoAcceleratorService::Result::ILLEGAL_STATE) ==
                  chromeos::arc::ArcVideoAccelerator::ILLEGAL_STATE,
              "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoAcceleratorService::Result::INVALID_ARGUMENT) ==
        chromeos::arc::ArcVideoAccelerator::INVALID_ARGUMENT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoAcceleratorService::Result::UNREADABLE_INPUT) ==
        chromeos::arc::ArcVideoAccelerator::UNREADABLE_INPUT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoAcceleratorService::Result::PLATFORM_FAILURE) ==
        chromeos::arc::ArcVideoAccelerator::PLATFORM_FAILURE,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoAcceleratorService::Result::INSUFFICIENT_RESOURCES) ==
        chromeos::arc::ArcVideoAccelerator::INSUFFICIENT_RESOURCES,
    "enum mismatch");

namespace mojo {

template <>
struct TypeConverter<arc::mojom::BufferMetadataPtr,
                     chromeos::arc::BufferMetadata> {
  static arc::mojom::BufferMetadataPtr Convert(
      const chromeos::arc::BufferMetadata& input) {
    arc::mojom::BufferMetadataPtr result = arc::mojom::BufferMetadata::New();
    result->timestamp = input.timestamp;
    result->bytes_used = input.bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<chromeos::arc::BufferMetadata,
                     arc::mojom::BufferMetadataPtr> {
  static chromeos::arc::BufferMetadata Convert(
      const arc::mojom::BufferMetadataPtr& input) {
    chromeos::arc::BufferMetadata result;
    result.timestamp = input->timestamp;
    result.bytes_used = input->bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<arc::mojom::VideoFormatPtr, chromeos::arc::VideoFormat> {
  static arc::mojom::VideoFormatPtr Convert(
      const chromeos::arc::VideoFormat& input) {
    arc::mojom::VideoFormatPtr result = arc::mojom::VideoFormat::New();
    result->pixel_format = input.pixel_format;
    result->buffer_size = input.buffer_size;
    result->min_num_buffers = input.min_num_buffers;
    result->coded_width = input.coded_width;
    result->coded_height = input.coded_height;
    result->crop_left = input.crop_left;
    result->crop_width = input.crop_width;
    result->crop_top = input.crop_top;
    result->crop_height = input.crop_height;
    return result;
  }
};

template <>
struct TypeConverter<chromeos::arc::ArcVideoAccelerator::Config,
                     arc::mojom::ArcVideoAcceleratorConfigPtr> {
  static chromeos::arc::ArcVideoAccelerator::Config Convert(
      const arc::mojom::ArcVideoAcceleratorConfigPtr& input) {
    chromeos::arc::ArcVideoAccelerator::Config result;
    result.device_type =
        static_cast<chromeos::arc::ArcVideoAccelerator::Config::DeviceType>(
            input->device_type);
    result.num_input_buffers = input->num_input_buffers;
    result.input_pixel_format = input->input_pixel_format;
    return result;
  }
};

}  // namespace mojo

namespace chromeos {
namespace arc {

GpuArcVideoService::GpuArcVideoService(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences),
      accelerator_(new ArcGpuVideoDecodeAccelerator(gpu_preferences_)) {}

GpuArcVideoService::~GpuArcVideoService() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GpuArcVideoService::OnError(ArcVideoAccelerator::Result error) {
  DVLOG(2) << "OnError " << error;
  DCHECK_NE(error, ArcVideoAccelerator::SUCCESS);
  DCHECK(client_);
  client_->OnError(
      static_cast<::arc::mojom::VideoAcceleratorService::Result>(error));
}

void GpuArcVideoService::OnBufferDone(PortType port,
                                      uint32_t index,
                                      const BufferMetadata& metadata) {
  DVLOG(2) << "OnBufferDone " << port << "," << index;
  DCHECK(client_);
  client_->OnBufferDone(static_cast<::arc::mojom::PortType>(port), index,
                        ::arc::mojom::BufferMetadata::From(metadata));
}

void GpuArcVideoService::OnFlushDone() {
  DVLOG(2) << "OnFlushDone";
  DCHECK(client_);
  client_->OnFlushDone();
}

void GpuArcVideoService::OnResetDone() {
  DVLOG(2) << "OnResetDone";
  DCHECK(client_);
  client_->OnResetDone();
}

void GpuArcVideoService::OnOutputFormatChanged(const VideoFormat& format) {
  DVLOG(2) << "OnOutputFormatChanged";
  DCHECK(client_);
  client_->OnOutputFormatChanged(::arc::mojom::VideoFormat::From(format));
}

void GpuArcVideoService::Initialize(
    ::arc::mojom::ArcVideoAcceleratorConfigPtr config,
    ::arc::mojom::VideoAcceleratorServiceClientPtr client,
    const InitializeCallback& callback) {
  DVLOG(2) << "Initialize";
  DCHECK(!client_);
  client_ = std::move(client);
  ArcVideoAccelerator::Result result =
      accelerator_->Initialize(config.To<ArcVideoAccelerator::Config>(), this);
  callback.Run(
      static_cast<::arc::mojom::VideoAcceleratorService::Result>(result));
}

base::ScopedFD GpuArcVideoService::UnwrapFdFromMojoHandle(
    mojo::ScopedHandle handle) {
  DCHECK(client_);
  if (!handle.is_valid()) {
    LOG(ERROR) << "handle is invalid";
    client_->OnError(
        ::arc::mojom::VideoAcceleratorService::Result::INVALID_ARGUMENT);
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "UnwrapPlatformFile failed: " << mojo_result;
    client_->OnError(
        ::arc::mojom::VideoAcceleratorService::Result::PLATFORM_FAILURE);
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

void GpuArcVideoService::BindSharedMemory(::arc::mojom::PortType port,
                                          uint32_t index,
                                          mojo::ScopedHandle ashmem_handle,
                                          uint32_t offset,
                                          uint32_t length) {
  DVLOG(2) << "BindSharedMemoryCallback port=" << port << ", index=" << index
           << ", offset=" << offset << ", length=" << length;

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(ashmem_handle));
  if (!fd.is_valid())
    return;
  accelerator_->BindSharedMemory(static_cast<PortType>(port), index,
                                 std::move(fd), offset, length);
}

void GpuArcVideoService::BindDmabuf(
    ::arc::mojom::PortType port,
    uint32_t index,
    mojo::ScopedHandle dmabuf_handle,
    std::vector<::arc::ArcVideoAcceleratorDmabufPlane> dmabuf_planes) {
  DVLOG(2) << "BindDmabuf port=" << port << ", index=" << index;

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(dmabuf_handle));
  if (!fd.is_valid())
    return;

  accelerator_->BindDmabuf(static_cast<PortType>(port), index, std::move(fd),
                           std::move(dmabuf_planes));
}

void GpuArcVideoService::UseBuffer(::arc::mojom::PortType port,
                                   uint32_t index,
                                   ::arc::mojom::BufferMetadataPtr metadata) {
  DVLOG(2) << "UseBuffer port=" << port << ", index=" << index;
  accelerator_->UseBuffer(static_cast<PortType>(port), index,
                          metadata.To<BufferMetadata>());
}

void GpuArcVideoService::SetNumberOfOutputBuffers(uint32_t number) {
  DVLOG(2) << "SetNumberOfOutputBuffers number=" << number;
  accelerator_->SetNumberOfOutputBuffers(number);
}

void GpuArcVideoService::Reset() {
  DVLOG(2) << "Reset";
  accelerator_->Reset();
}

void GpuArcVideoService::Flush() {
  DVLOG(2) << "Flush";
  accelerator_->Flush();
}

}  // namespace arc
}  // namespace chromeos
