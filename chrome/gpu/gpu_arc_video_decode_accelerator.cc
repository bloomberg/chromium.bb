// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_arc_video_decode_accelerator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/gpu/chrome_arc_video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Make sure arc::mojom::VideoDecodeAccelerator::Result and
// chromeos::arc::ArcVideoDecodeAccelerator::Result match.
static_assert(
    static_cast<int>(arc::mojom::VideoDecodeAccelerator::Result::SUCCESS) ==
        chromeos::arc::ArcVideoDecodeAccelerator::SUCCESS,
    "enum mismatch");
static_assert(static_cast<int>(
                  arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE) ==
                  chromeos::arc::ArcVideoDecodeAccelerator::ILLEGAL_STATE,
              "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT) ==
        chromeos::arc::ArcVideoDecodeAccelerator::INVALID_ARGUMENT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT) ==
        chromeos::arc::ArcVideoDecodeAccelerator::UNREADABLE_INPUT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE) ==
        chromeos::arc::ArcVideoDecodeAccelerator::PLATFORM_FAILURE,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::INSUFFICIENT_RESOURCES) ==
        chromeos::arc::ArcVideoDecodeAccelerator::INSUFFICIENT_RESOURCES,
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
struct TypeConverter<chromeos::arc::ArcVideoDecodeAccelerator::Config,
                     arc::mojom::VideoDecodeAcceleratorConfigPtr> {
  static chromeos::arc::ArcVideoDecodeAccelerator::Config Convert(
      const arc::mojom::VideoDecodeAcceleratorConfigPtr& input) {
    chromeos::arc::ArcVideoDecodeAccelerator::Config result;
    result.num_input_buffers = input->num_input_buffers;
    result.input_pixel_format = input->input_pixel_format;
    return result;
  }
};

}  // namespace mojo

namespace chromeos {
namespace arc {

GpuArcVideoDecodeAccelerator::GpuArcVideoDecodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences)
    : gpu_preferences_(gpu_preferences),
      accelerator_(new ChromeArcVideoDecodeAccelerator(gpu_preferences_)) {}

GpuArcVideoDecodeAccelerator::~GpuArcVideoDecodeAccelerator() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void GpuArcVideoDecodeAccelerator::OnError(
    ArcVideoDecodeAccelerator::Result error) {
  DVLOG(2) << "OnError " << error;
  DCHECK_NE(error, ArcVideoDecodeAccelerator::SUCCESS);
  DCHECK(client_);
  client_->OnError(
      static_cast<::arc::mojom::VideoDecodeAccelerator::Result>(error));
}

void GpuArcVideoDecodeAccelerator::OnBufferDone(
    PortType port,
    uint32_t index,
    const BufferMetadata& metadata) {
  DVLOG(2) << "OnBufferDone " << port << "," << index;
  DCHECK(client_);
  client_->OnBufferDone(static_cast<::arc::mojom::PortType>(port), index,
                        ::arc::mojom::BufferMetadata::From(metadata));
}

void GpuArcVideoDecodeAccelerator::OnFlushDone() {
  DVLOG(2) << "OnFlushDone";
  DCHECK(client_);
  client_->OnFlushDone();
}

void GpuArcVideoDecodeAccelerator::OnResetDone() {
  DVLOG(2) << "OnResetDone";
  DCHECK(client_);
  client_->OnResetDone();
}

void GpuArcVideoDecodeAccelerator::OnOutputFormatChanged(
    const VideoFormat& format) {
  DVLOG(2) << "OnOutputFormatChanged";
  DCHECK(client_);
  client_->OnOutputFormatChanged(::arc::mojom::VideoFormat::From(format));
}

void GpuArcVideoDecodeAccelerator::Initialize(
    ::arc::mojom::VideoDecodeAcceleratorConfigPtr config,
    ::arc::mojom::VideoDecodeClientPtr client,
    const InitializeCallback& callback) {
  DVLOG(2) << "Initialize";
  DCHECK(!client_);
  if (config->device_type_deprecated !=
      ::arc::mojom::VideoDecodeAcceleratorConfig::DeviceTypeDeprecated::
          DEVICE_DECODER) {
    LOG(ERROR) << "only decoder is supported";
    callback.Run(
        ::arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
  }
  client_ = std::move(client);
  ArcVideoDecodeAccelerator::Result result = accelerator_->Initialize(
      config.To<ArcVideoDecodeAccelerator::Config>(), this);
  callback.Run(
      static_cast<::arc::mojom::VideoDecodeAccelerator::Result>(result));
}

base::ScopedFD GpuArcVideoDecodeAccelerator::UnwrapFdFromMojoHandle(
    mojo::ScopedHandle handle) {
  DCHECK(client_);
  if (!handle.is_valid()) {
    LOG(ERROR) << "handle is invalid";
    client_->OnError(
        ::arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT);
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "UnwrapPlatformFile failed: " << mojo_result;
    client_->OnError(
        ::arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE);
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

void GpuArcVideoDecodeAccelerator::BindSharedMemory(
    ::arc::mojom::PortType port,
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

void GpuArcVideoDecodeAccelerator::BindDmabuf(
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

void GpuArcVideoDecodeAccelerator::UseBuffer(
    ::arc::mojom::PortType port,
    uint32_t index,
    ::arc::mojom::BufferMetadataPtr metadata) {
  DVLOG(2) << "UseBuffer port=" << port << ", index=" << index;
  accelerator_->UseBuffer(static_cast<PortType>(port), index,
                          metadata.To<BufferMetadata>());
}

void GpuArcVideoDecodeAccelerator::SetNumberOfOutputBuffers(uint32_t number) {
  DVLOG(2) << "SetNumberOfOutputBuffers number=" << number;
  accelerator_->SetNumberOfOutputBuffers(number);
}

void GpuArcVideoDecodeAccelerator::Reset() {
  DVLOG(2) << "Reset";
  accelerator_->Reset();
}

void GpuArcVideoDecodeAccelerator::Flush() {
  DVLOG(2) << "Flush";
  accelerator_->Flush();
}

}  // namespace arc
}  // namespace chromeos
