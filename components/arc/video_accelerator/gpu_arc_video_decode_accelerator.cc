// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/gpu_arc_video_decode_accelerator.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/video_accelerator/chrome_arc_video_decode_accelerator.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Make sure arc::mojom::VideoDecodeAccelerator::Result and
// arc::ArcVideoDecodeAccelerator::Result match.
static_assert(
    static_cast<int>(arc::mojom::VideoDecodeAccelerator::Result::SUCCESS) ==
        arc::ArcVideoDecodeAccelerator::SUCCESS,
    "enum mismatch");
static_assert(static_cast<int>(
                  arc::mojom::VideoDecodeAccelerator::Result::ILLEGAL_STATE) ==
                  arc::ArcVideoDecodeAccelerator::ILLEGAL_STATE,
              "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::INVALID_ARGUMENT) ==
        arc::ArcVideoDecodeAccelerator::INVALID_ARGUMENT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::UNREADABLE_INPUT) ==
        arc::ArcVideoDecodeAccelerator::UNREADABLE_INPUT,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::PLATFORM_FAILURE) ==
        arc::ArcVideoDecodeAccelerator::PLATFORM_FAILURE,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAccelerator::Result::INSUFFICIENT_RESOURCES) ==
        arc::ArcVideoDecodeAccelerator::INSUFFICIENT_RESOURCES,
    "enum mismatch");

namespace mojo {

template <>
struct TypeConverter<arc::mojom::BufferMetadataPtr, arc::BufferMetadata> {
  static arc::mojom::BufferMetadataPtr Convert(
      const arc::BufferMetadata& input) {
    arc::mojom::BufferMetadataPtr result = arc::mojom::BufferMetadata::New();
    result->timestamp = input.timestamp;
    result->bytes_used = input.bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<arc::BufferMetadata, arc::mojom::BufferMetadataPtr> {
  static arc::BufferMetadata Convert(
      const arc::mojom::BufferMetadataPtr& input) {
    arc::BufferMetadata result;
    result.timestamp = input->timestamp;
    result.bytes_used = input->bytes_used;
    return result;
  }
};

template <>
struct TypeConverter<arc::mojom::VideoFormatPtr, arc::VideoFormat> {
  static arc::mojom::VideoFormatPtr Convert(const arc::VideoFormat& input) {
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
struct TypeConverter<arc::ArcVideoDecodeAccelerator::Config,
                     arc::mojom::VideoDecodeAcceleratorConfigPtr> {
  static arc::ArcVideoDecodeAccelerator::Config Convert(
      const arc::mojom::VideoDecodeAcceleratorConfigPtr& input) {
    arc::ArcVideoDecodeAccelerator::Config result;
    result.num_input_buffers = input->num_input_buffers;
    result.input_pixel_format = input->input_pixel_format;
    result.secure_mode = input->secure_mode;
    return result;
  }
};

}  // namespace mojo

namespace arc {

GpuArcVideoDecodeAccelerator::GpuArcVideoDecodeAccelerator(
    const gpu::GpuPreferences& gpu_preferences,
    ProtectedBufferManager* protected_buffer_manager)
    : gpu_preferences_(gpu_preferences),
      accelerator_(std::make_unique<ChromeArcVideoDecodeAccelerator>(
          gpu_preferences_,
          protected_buffer_manager)) {}

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
    InitializeCallback callback) {
  DVLOG(2) << "Initialize";
  DCHECK(!client_);

  client_ = std::move(client);
  ArcVideoDecodeAccelerator::Result result = accelerator_->Initialize(
      config.To<ArcVideoDecodeAccelerator::Config>(), this);
  std::move(callback).Run(
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

void GpuArcVideoDecodeAccelerator::AllocateProtectedBuffer(
    ::arc::mojom::PortType port,
    uint32_t index,
    mojo::ScopedHandle handle,
    uint64_t size,
    AllocateProtectedBufferCallback callback) {
  DVLOG(2) << "port=" << port << ", index=" << index << ", size=" << size;

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!fd.is_valid()) {
    std::move(callback).Run(false);
    return;
  }

  bool result = accelerator_->AllocateProtectedBuffer(
      static_cast<PortType>(port), index, std::move(fd), size);

  std::move(callback).Run(result);
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
    std::vector<::arc::VideoFramePlane> planes) {
  DVLOG(2) << "BindDmabuf port=" << port << ", index=" << index;

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(dmabuf_handle));
  if (!fd.is_valid())
    return;

  accelerator_->BindDmabuf(static_cast<PortType>(port), index, std::move(fd),
                           std::move(planes));
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
