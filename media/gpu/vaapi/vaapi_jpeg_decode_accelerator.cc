// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_jpeg_decode_accelerator.h"

#include <stddef.h>

#include <iostream>
#include <utility>
#include <vector>

#include <va/va.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/filters/jpeg_parser.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/vaapi_jpeg_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

namespace {

// TODO(andrescj): remove this once VaapiJpegDecoder is responsible for
// obtaining the VAImage.
constexpr VAImageFormat kImageFormatI420 = {
    .fourcc = VA_FOURCC_I420,
    .byte_order = VA_LSB_FIRST,
    .bits_per_pixel = 12,
};

// UMA errors that the VaapiJpegDecodeAccelerator class reports.
enum VAJDADecoderFailure {
  VAAPI_ERROR = 0,
  VAJDA_DECODER_FAILURES_MAX,
};

static void ReportToUMA(VAJDADecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJDA.DecoderFailure", failure,
                            VAJDA_DECODER_FAILURES_MAX + 1);
}

}  // namespace

void VaapiJpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                             Error error) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VaapiJpegDecodeAccelerator::NotifyError,
                                  weak_this_factory_.GetWeakPtr(),
                                  bitstream_buffer_id, error));
    return;
  }
  VLOGF(1) << "Notifying of error " << error;
  DCHECK(client_);
  client_->NotifyError(bitstream_buffer_id, error);
}

void VaapiJpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(bitstream_buffer_id);
}

VaapiJpegDecodeAccelerator::VaapiJpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      decoder_thread_("VaapiJpegDecoderThread"),
      va_surface_id_(VA_INVALID_SURFACE),
      va_rt_format_(0),
      weak_this_factory_(this) {}

VaapiJpegDecodeAccelerator::~VaapiJpegDecodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  VLOGF(2) << "Destroying VaapiJpegDecodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  decoder_thread_.Stop();
}

bool VaapiJpegDecodeAccelerator::Initialize(Client* client) {
  VLOGF(2);
  DCHECK(task_runner_->BelongsToCurrentThread());

  client_ = client;

  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, VAProfileJPEGBaseline,
                           base::Bind(&ReportToUMA, VAAPI_ERROR));

  if (!vaapi_wrapper_.get()) {
    VLOGF(1) << "Failed initializing VAAPI";
    return false;
  }

  if (!decoder_thread_.Start()) {
    VLOGF(1) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

bool VaapiJpegDecodeAccelerator::OutputPicture(
    VASurfaceID va_surface_id,
    uint32_t va_surface_format,
    int32_t input_buffer_id,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  TRACE_EVENT1("jpeg", "VaapiJpegDecodeAccelerator::OutputPicture",
               "input_buffer_id", input_buffer_id);

  DVLOGF(4) << "Outputting VASurface " << va_surface_id
            << " into video_frame associated with input buffer id "
            << input_buffer_id;

  // Specify which image format we will request from the VAAPI. As the expected
  // output format is I420, we will first try this format. If converting to I420
  // is not supported by the decoder, we will request the image in its original
  // chroma sampling format.
  VAImageFormat va_image_format = kImageFormatI420;
  if (!VaapiWrapper::IsImageFormatSupported(va_image_format)) {
    if (!VaSurfaceFormatToImageFormat(va_surface_format, &va_image_format)) {
      VLOGF(1) << "Unsupported surface format";
      return false;
    }
  }

  const gfx::Size coded_size = video_frame->coded_size();
  auto scoped_image = vaapi_wrapper_->CreateVaImage(
      va_surface_id, &va_image_format, coded_size);
  if (!scoped_image) {
    VLOGF(1) << "Cannot get VAImage";
    return false;
  }
  const VAImage* image = scoped_image->image();
  auto* mem = static_cast<uint8_t*>(scoped_image->va_buffer()->data());

  // Copy image content from VAImage to VideoFrame. If the image is not in the
  // I420 format we'll have to convert it.
  DCHECK_GE(image->width, coded_size.width());
  DCHECK_GE(image->height, coded_size.height());
  uint8_t* dst_y = video_frame->data(VideoFrame::kYPlane);
  uint8_t* dst_u = video_frame->data(VideoFrame::kUPlane);
  uint8_t* dst_v = video_frame->data(VideoFrame::kVPlane);
  size_t dst_y_stride = video_frame->stride(VideoFrame::kYPlane);
  size_t dst_u_stride = video_frame->stride(VideoFrame::kUPlane);
  size_t dst_v_stride = video_frame->stride(VideoFrame::kVPlane);

  switch (va_image_format.fourcc) {
    case VA_FOURCC_I420: {
      DCHECK_EQ(image->num_planes, 3u);
      const uint8_t* src_y = mem + image->offsets[0];
      const uint8_t* src_u = mem + image->offsets[1];
      const uint8_t* src_v = mem + image->offsets[2];
      const size_t src_y_stride = image->pitches[0];
      const size_t src_u_stride = image->pitches[1];
      const size_t src_v_stride = image->pitches[2];
      if (libyuv::I420Copy(src_y, src_y_stride, src_u, src_u_stride, src_v,
                           src_v_stride, dst_y, dst_y_stride, dst_u,
                           dst_u_stride, dst_v, dst_v_stride,
                           coded_size.width(), coded_size.height())) {
        VLOGF(1) << "I420Copy failed";
        return false;
      }
      break;
    }
    case VA_FOURCC_YUY2:
    case VA_FOURCC('Y', 'U', 'Y', 'V'): {
      DCHECK_EQ(image->num_planes, 1u);
      const uint8_t* src_yuy2 = mem + image->offsets[0];
      const size_t src_yuy2_stride = image->pitches[0];
      if (libyuv::YUY2ToI420(src_yuy2, src_yuy2_stride, dst_y, dst_y_stride,
                             dst_u, dst_u_stride, dst_v, dst_v_stride,
                             coded_size.width(), coded_size.height())) {
        VLOGF(1) << "YUY2ToI420 failed";
        return false;
      }
      break;
    }
    default:
      VLOGF(1) << "Can't convert image to I420: unsupported format 0x"
               << std::hex << va_image_format.fourcc;
      return false;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VaapiJpegDecodeAccelerator::VideoFrameReady,
                     weak_this_factory_.GetWeakPtr(), input_buffer_id));

  return true;
}

void VaapiJpegDecodeAccelerator::DecodeTask(
    int32_t bitstream_buffer_id,
    std::unique_ptr<UnalignedSharedMemory> shm,
    scoped_refptr<VideoFrame> video_frame) {
  DVLOGF(4);
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT0("jpeg", "DecodeTask");

  JpegParseResult parse_result;
  if (!ParseJpegPicture(static_cast<const uint8_t*>(shm->memory()), shm->size(),
                        &parse_result)) {
    VLOGF(1) << "ParseJpegPicture failed";
    NotifyError(bitstream_buffer_id, PARSE_JPEG_FAILED);
    return;
  }

  const uint32_t picture_va_rt_format =
      VaSurfaceFormatForJpeg(parse_result.frame_header);
  if (!picture_va_rt_format) {
    VLOGF(1) << "Unsupported subsampling";
    NotifyError(bitstream_buffer_id, UNSUPPORTED_JPEG);
    return;
  }

  // Reuse VASurface if size doesn't change.
  gfx::Size new_coded_size(parse_result.frame_header.coded_width,
                           parse_result.frame_header.coded_height);
  if (new_coded_size != coded_size_ || va_surface_id_ == VA_INVALID_SURFACE ||
      picture_va_rt_format != va_rt_format_) {
    vaapi_wrapper_->DestroyContextAndSurfaces();
    va_surface_id_ = VA_INVALID_SURFACE;
    va_rt_format_ = picture_va_rt_format;

    std::vector<VASurfaceID> va_surfaces;
    if (!vaapi_wrapper_->CreateContextAndSurfaces(va_rt_format_, new_coded_size,
                                                  1, &va_surfaces)) {
      VLOGF(1) << "Create VA surface failed";
      NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
      return;
    }
    va_surface_id_ = va_surfaces[0];
    coded_size_ = new_coded_size;
  }

  if (!VaapiJpegDecoder::DoDecode(vaapi_wrapper_.get(), parse_result,
                                  va_surface_id_)) {
    VLOGF(1) << "Decode JPEG failed";
    NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
    return;
  }

  if (!OutputPicture(va_surface_id_, picture_va_rt_format, bitstream_buffer_id,
                     video_frame)) {
    VLOGF(1) << "Output picture failed";
    NotifyError(bitstream_buffer_id, PLATFORM_FAILURE);
    return;
  }
}

void VaapiJpegDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<VideoFrame>& video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  TRACE_EVENT1("jpeg", "Decode", "input_id", bitstream_buffer.id());

  DVLOGF(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
            << " size: " << bitstream_buffer.size();

  // UnalignedSharedMemory will take over the |bitstream_buffer.handle()|.
  auto shm = std::make_unique<UnalignedSharedMemory>(
      bitstream_buffer.handle(), bitstream_buffer.size(), true);

  if (bitstream_buffer.id() < 0) {
    VLOGF(1) << "Invalid bitstream_buffer, id: " << bitstream_buffer.id();
    NotifyError(bitstream_buffer.id(), INVALID_ARGUMENT);
    return;
  }

  if (!shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size())) {
    VLOGF(1) << "Failed to map input buffer";
    NotifyError(bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }

  // It's safe to use base::Unretained(this) because |decoder_task_runner_| runs
  // tasks on |decoder_thread_| which is stopped in the destructor of |this|.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VaapiJpegDecodeAccelerator::DecodeTask,
                                base::Unretained(this), bitstream_buffer.id(),
                                std::move(shm), std::move(video_frame)));
}

bool VaapiJpegDecodeAccelerator::IsSupported() {
  return VaapiWrapper::IsJpegDecodeSupported();
}

}  // namespace media
