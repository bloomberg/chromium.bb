// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "media/gpu/format_utils.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_picture_factory.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/picture.h"

#if defined(OS_LINUX)
#include "media/gpu/linux/platform_video_frame_utils.h"
#endif

namespace media {

namespace {

constexpr VAImageFormat kImageFormatNV12{.fourcc = VA_FOURCC_NV12,
                                         .byte_order = VA_LSB_FIRST,
                                         .bits_per_pixel = 12};

void DeallocateBuffers(std::unique_ptr<ScopedVAImage> va_image) {
  // Destructing ScopedVAImage releases its owned memory.
  DCHECK(va_image->IsValid());
}

scoped_refptr<VideoFrame> CreateMappedVideoFrame(
    const VideoFrameLayout& layout,
    const gfx::Rect& visible_rect,
    std::unique_ptr<ScopedVAImage> va_image) {
  // ScopedVAImage manages the resource of mapped data. That is, ScopedVAImage's
  // dtor releases the mapped resource.
  const size_t num_planes = layout.num_planes();
  if (num_planes != va_image->image()->num_planes) {
    VLOGF(1) << "The number of planes is not same between layout and VAImage, "
             << "(layout: " << num_planes
             << ", VAImage: " << va_image->image()->num_planes << ")";
    return nullptr;
  }

  // All the planes are stored in the same buffer, VAImage.va_buffer.
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  std::vector<uint8_t*> addrs(num_planes, nullptr);
  for (size_t i = 0; i < num_planes; i++) {
    planes[i].stride = va_image->image()->pitches[i];
    planes[i].offset = va_image->image()->offsets[i];
    addrs[i] = static_cast<uint8_t*>(va_image->va_buffer()->data()) +
               va_image->image()->offsets[i];
  }

  auto mapped_layout = VideoFrameLayout::CreateWithPlanes(
      layout.format(),
      gfx::Size(va_image->image()->width, va_image->image()->height),
      std::move(planes), {va_image->image()->data_size});
  if (!mapped_layout) {
    VLOGF(1) << "Failed to create VideoFrameLayout for VAImage";
    return nullptr;
  }
  auto video_frame = VideoFrame::WrapExternalYuvDataWithLayout(
      *mapped_layout, visible_rect, visible_rect.size(), addrs[0], addrs[1],
      addrs[2], base::TimeDelta());
  if (!video_frame)
    return nullptr;

  video_frame->AddDestructionObserver(
      base::BindOnce(DeallocateBuffers, std::move(va_image)));
  return video_frame;
}

}  // namespace

// static
std::unique_ptr<VideoFrameMapper> VaapiDmaBufVideoFrameMapper::Create() {
  auto video_frame_mapper = base::WrapUnique(new VaapiDmaBufVideoFrameMapper);
  if (video_frame_mapper->vaapi_wrapper_ == nullptr) {
    return nullptr;
  }
  return video_frame_mapper;
}

// While kDecode and H264PROFILE_MAIN are set here,  the mode and profile are
// not required for VaapiWrapper to perform pixel format conversion.
// TODO(crbug.com/898423): Create a VaapiWrapper only for pixel format
// conversion. Either mode or profile isn't required to create the VaapiWrapper.
VaapiDmaBufVideoFrameMapper::VaapiDmaBufVideoFrameMapper()
    : vaapi_wrapper_(VaapiWrapper::CreateForVideoCodec(VaapiWrapper::kDecode,
                                                       H264PROFILE_MAIN,
                                                       base::DoNothing())),
      vaapi_picture_factory_(new VaapiPictureFactory()) {}

VaapiDmaBufVideoFrameMapper::~VaapiDmaBufVideoFrameMapper() {}

scoped_refptr<VideoFrame> VaapiDmaBufVideoFrameMapper::Map(
    scoped_refptr<const VideoFrame> video_frame) const {
  DCHECK(vaapi_wrapper_);
  DCHECK(vaapi_picture_factory_);
  if (!video_frame->HasDmaBufs()) {
    return nullptr;
  }
  if (video_frame->format() != PIXEL_FORMAT_NV12) {
    NOTIMPLEMENTED() << " Unsupported PixelFormat: " << video_frame->format();
    return nullptr;
  }

  const gfx::Size& coded_size = video_frame->coded_size();
  constexpr int32_t kDummyPictureBufferId = 0;

  // Passing empty callbacks is ok, because given PictureBuffer doesn't have
  // texture id and thus these callbacks will never called.
  auto va_picture = vaapi_picture_factory_->Create(
      vaapi_wrapper_, MakeGLContextCurrentCallback(), BindGLImageCallback(),
      PictureBuffer(kDummyPictureBufferId, coded_size));
  if (!va_picture) {
    VLOGF(1) << "Failed to create VaapiPicture.";
    return nullptr;
  }

  gfx::GpuMemoryBufferHandle gmb_handle;
#if defined(OS_LINUX)
  gmb_handle = CreateGpuMemoryBufferHandle(video_frame.get());
#endif
  if (gmb_handle.is_null()) {
    VLOGF(1) << "Failed to CreateGMBHandleFromVideoFrame.";
    return nullptr;
  }
  if (!va_picture->ImportGpuMemoryBufferHandle(
          VideoPixelFormatToGfxBufferFormat(video_frame->format()),
          std::move(gmb_handle))) {
    VLOGF(1) << "Failed in ImportGpuMemoryBufferHandle.";
    return nullptr;
  }

  // Map tiled NV12 buffer by CreateVaImage so that mapped buffers can be
  // accessed as non-tiled NV12 buffer.
  constexpr VideoPixelFormat kConvertedFormat = PIXEL_FORMAT_NV12;
  VAImageFormat va_image_format = kImageFormatNV12;
  auto va_image = vaapi_wrapper_->CreateVaImage(
      va_picture->va_surface_id(), &va_image_format, video_frame->coded_size());
  if (!va_image || !va_image->IsValid()) {
    VLOGF(1) << "Failed in CreateVaImage.";
    return nullptr;
  }

  auto layout =
      VideoFrameLayout::Create(kConvertedFormat, video_frame->coded_size());
  if (!layout) {
    VLOGF(1) << "Failed to create VideoFrameLayout.";
    return nullptr;
  }
  return CreateMappedVideoFrame(*layout, video_frame->visible_rect(),
                                std::move(va_image));
}

}  // namespace media
