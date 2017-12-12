// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/gpu_arc_video_decode_accelerator_deprecated.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/arc/video_accelerator/chrome_arc_video_decode_accelerator_deprecated.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"

// Make sure arc::mojom::VideoDecodeAccelerator::Result and
// arc::ArcVideoDecodeAccelerator::Result match.
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAcceleratorDeprecated::Result::SUCCESS) ==
        arc::ArcVideoDecodeAcceleratorDeprecated::SUCCESS,
    "enum mismatch");
static_assert(
    static_cast<int>(
        arc::mojom::VideoDecodeAcceleratorDeprecated::Result::ILLEGAL_STATE) ==
        arc::ArcVideoDecodeAcceleratorDeprecated::ILLEGAL_STATE,
    "enum mismatch");
static_assert(static_cast<int>(arc::mojom::VideoDecodeAcceleratorDeprecated::
                                   Result::INVALID_ARGUMENT) ==
                  arc::ArcVideoDecodeAcceleratorDeprecated::INVALID_ARGUMENT,
              "enum mismatch");
static_assert(static_cast<int>(arc::mojom::VideoDecodeAcceleratorDeprecated::
                                   Result::UNREADABLE_INPUT) ==
                  arc::ArcVideoDecodeAcceleratorDeprecated::UNREADABLE_INPUT,
              "enum mismatch");
static_assert(static_cast<int>(arc::mojom::VideoDecodeAcceleratorDeprecated::
                                   Result::PLATFORM_FAILURE) ==
                  arc::ArcVideoDecodeAcceleratorDeprecated::PLATFORM_FAILURE,
              "enum mismatch");
static_assert(
    static_cast<int>(arc::mojom::VideoDecodeAcceleratorDeprecated::Result::
                         INSUFFICIENT_RESOURCES) ==
        arc::ArcVideoDecodeAcceleratorDeprecated::INSUFFICIENT_RESOURCES,
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
struct TypeConverter<arc::ArcVideoDecodeAcceleratorDeprecated::Config,
                     arc::mojom::VideoDecodeAcceleratorConfigDeprecatedPtr> {
  static arc::ArcVideoDecodeAcceleratorDeprecated::Config Convert(
      const arc::mojom::VideoDecodeAcceleratorConfigDeprecatedPtr& input) {
    arc::ArcVideoDecodeAcceleratorDeprecated::Config result;
    result.num_input_buffers = input->num_input_buffers;
    result.input_pixel_format = input->input_pixel_format;
    result.secure_mode = input->secure_mode;
    return result;
  }
};

}  // namespace mojo

namespace arc {

GpuArcVideoDecodeAcceleratorDeprecated::GpuArcVideoDecodeAcceleratorDeprecated(
    const gpu::GpuPreferences& gpu_preferences,
    ProtectedBufferManager* protected_buffer_manager)
    : gpu_preferences_(gpu_preferences),
      accelerator_(std::make_unique<ChromeArcVideoDecodeAcceleratorDeprecated>(
          gpu_preferences_,
          protected_buffer_manager)) {}

GpuArcVideoDecodeAcceleratorDeprecated::
    ~GpuArcVideoDecodeAcceleratorDeprecated() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void GpuArcVideoDecodeAcceleratorDeprecated::OnError(
    ArcVideoDecodeAcceleratorDeprecated::Result error) {
  DVLOG(2) << "OnError " << error;
  DCHECK_NE(error, ArcVideoDecodeAcceleratorDeprecated::SUCCESS);
  DCHECK(client_);
  client_->OnError(
      static_cast<::arc::mojom::VideoDecodeAcceleratorDeprecated::Result>(
          error));
}

void GpuArcVideoDecodeAcceleratorDeprecated::OnBufferDone(
    PortType port,
    uint32_t index,
    const BufferMetadata& metadata) {
  DVLOG(2) << "OnBufferDone " << port << "," << index;
  DCHECK(client_);
  client_->OnBufferDone(static_cast<::arc::mojom::PortType>(port), index,
                        ::arc::mojom::BufferMetadata::From(metadata));
}

void GpuArcVideoDecodeAcceleratorDeprecated::OnFlushDone() {
  DVLOG(2) << "OnFlushDone";
  DCHECK(client_);
  client_->OnFlushDone();
}

void GpuArcVideoDecodeAcceleratorDeprecated::OnResetDone() {
  DVLOG(2) << "OnResetDone";
  DCHECK(client_);
  client_->OnResetDone();
}

void GpuArcVideoDecodeAcceleratorDeprecated::OnOutputFormatChanged(
    const VideoFormat& format) {
  DVLOG(2) << "OnOutputFormatChanged";
  DCHECK(client_);
  client_->OnOutputFormatChanged(::arc::mojom::VideoFormat::From(format));
}

void GpuArcVideoDecodeAcceleratorDeprecated::Initialize(
    ::arc::mojom::VideoDecodeAcceleratorConfigDeprecatedPtr config,
    ::arc::mojom::VideoDecodeClientDeprecatedPtr client,
    InitializeCallback callback) {
  DVLOG(2) << "Initialize";
  DCHECK(!client_);

  client_ = std::move(client);
  ArcVideoDecodeAcceleratorDeprecated::Result result = accelerator_->Initialize(
      config.To<ArcVideoDecodeAcceleratorDeprecated::Config>(), this);
  std::move(callback).Run(
      static_cast<::arc::mojom::VideoDecodeAcceleratorDeprecated::Result>(
          result));
}

base::ScopedFD GpuArcVideoDecodeAcceleratorDeprecated::UnwrapFdFromMojoHandle(
    mojo::ScopedHandle handle) {
  DCHECK(client_);
  if (!handle.is_valid()) {
    LOG(ERROR) << "handle is invalid";
    client_->OnError(::arc::mojom::VideoDecodeAcceleratorDeprecated::Result::
                         INVALID_ARGUMENT);
    return base::ScopedFD();
  }

  base::PlatformFile platform_file;
  MojoResult mojo_result =
      mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  if (mojo_result != MOJO_RESULT_OK) {
    LOG(ERROR) << "UnwrapPlatformFile failed: " << mojo_result;
    client_->OnError(::arc::mojom::VideoDecodeAcceleratorDeprecated::Result::
                         PLATFORM_FAILURE);
    return base::ScopedFD();
  }

  return base::ScopedFD(platform_file);
}

void GpuArcVideoDecodeAcceleratorDeprecated::AllocateProtectedBuffer(
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

void GpuArcVideoDecodeAcceleratorDeprecated::BindSharedMemory(
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

void GpuArcVideoDecodeAcceleratorDeprecated::BindDmabuf(
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

void GpuArcVideoDecodeAcceleratorDeprecated::UseBuffer(
    ::arc::mojom::PortType port,
    uint32_t index,
    ::arc::mojom::BufferMetadataPtr metadata) {
  DVLOG(2) << "UseBuffer port=" << port << ", index=" << index;
  accelerator_->UseBuffer(static_cast<PortType>(port), index,
                          metadata.To<BufferMetadata>());
}

void GpuArcVideoDecodeAcceleratorDeprecated::SetNumberOfOutputBuffers(
    uint32_t number) {
  DVLOG(2) << "SetNumberOfOutputBuffers number=" << number;
  accelerator_->SetNumberOfOutputBuffers(number);
}

void GpuArcVideoDecodeAcceleratorDeprecated::Reset() {
  DVLOG(2) << "Reset";
  accelerator_->Reset();
}

void GpuArcVideoDecodeAcceleratorDeprecated::Flush() {
  DVLOG(2) << "Flush";
  accelerator_->Flush();
}

}  // namespace arc
