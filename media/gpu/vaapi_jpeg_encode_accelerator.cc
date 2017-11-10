// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi_jpeg_encode_accelerator.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/gpu/vaapi_jpeg_encoder.h"

namespace media {

namespace {

// UMA results that the VaapiJpegEncodeAccelerator class reports.
// These values are persisted to logs, and should therefore never be renumbered
// nor reused.
enum VAJEAEncoderResult {
  VAAPI_SUCCESS = 0,
  VAAPI_ERROR,
  VAJEA_ENCODER_RESULT_MAX = VAAPI_ERROR,
};

static void ReportToUMA(VAJEAEncoderResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.VAJEA.EncoderResult", result,
                            VAJEAEncoderResult::VAJEA_ENCODER_RESULT_MAX + 1);
}
}  // namespace

VaapiJpegEncodeAccelerator::EncodeRequest::EncodeRequest(
    scoped_refptr<media::VideoFrame> video_frame,
    std::unique_ptr<SharedMemoryRegion> shm,
    int quality)
    : video_frame(std::move(video_frame)),
      shm(std::move(shm)),
      quality(quality) {}

VaapiJpegEncodeAccelerator::EncodeRequest::~EncodeRequest() {}

class VaapiJpegEncodeAccelerator::Encoder {
 public:
  Encoder(scoped_refptr<VaapiWrapper> vaapi_wrapper,
          base::RepeatingCallback<void(int, size_t)> video_frame_ready_cb,
          base::RepeatingCallback<void(int, Status)> notify_error_cb);
  ~Encoder();

  // Processes one encode |request|.
  void EncodeTask(std::unique_ptr<EncodeRequest> request);

 private:
  // |cached_output_buffer_id_| is the last allocated VABuffer during
  // EncodeTask() and |cached_output_buffer_size_| is the size of it.
  // If the next call to EncodeTask() does not require a buffer bigger than
  // |cached_output_buffer_size_|, |cached_output_buffer_id_| will be reused.
  size_t cached_output_buffer_size_;
  VABufferID cached_output_buffer_id_;

  std::unique_ptr<VaapiJpegEncoder> jpeg_encoder_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  base::RepeatingCallback<void(int, size_t)> video_frame_ready_cb_;
  base::RepeatingCallback<void(int, Status)> notify_error_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Encoder);
};

VaapiJpegEncodeAccelerator::Encoder::Encoder(
    scoped_refptr<VaapiWrapper> vaapi_wrapper,
    base::RepeatingCallback<void(int, size_t)> video_frame_ready_cb,
    base::RepeatingCallback<void(int, Status)> notify_error_cb)
    : cached_output_buffer_size_(0),
      jpeg_encoder_(new VaapiJpegEncoder(vaapi_wrapper)),
      vaapi_wrapper_(std::move(vaapi_wrapper)),
      video_frame_ready_cb_(std::move(video_frame_ready_cb)),
      notify_error_cb_(std::move(notify_error_cb)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VaapiJpegEncodeAccelerator::Encoder::~Encoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VaapiJpegEncodeAccelerator::Encoder::EncodeTask(
    std::unique_ptr<EncodeRequest> request) {
  TRACE_EVENT0("jpeg", "EncodeTask");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const int video_frame_id = request->video_frame->unique_id();
  gfx::Size input_size = request->video_frame->coded_size();
  std::vector<VASurfaceID> va_surfaces;
  if (!vaapi_wrapper_->CreateSurfaces(VA_RT_FORMAT_YUV420, input_size, 1,
                                      &va_surfaces)) {
    VLOG(1) << "Failed to create VA surface";
    notify_error_cb_.Run(video_frame_id, PLATFORM_FAILURE);
    return;
  }
  VASurfaceID va_surface_id = va_surfaces[0];

  if (!vaapi_wrapper_->UploadVideoFrameToSurface(request->video_frame,
                                                 va_surface_id)) {
    VLOG(1) << "Failed to upload video frame to VA surface";
    notify_error_cb_.Run(video_frame_id, PLATFORM_FAILURE);
    return;
  }

  // Create output buffer for encoding result.
  size_t max_coded_buffer_size =
      VaapiJpegEncoder::GetMaxCodedBufferSize(input_size);
  if (max_coded_buffer_size > cached_output_buffer_size_) {
    vaapi_wrapper_->DestroyCodedBuffers();
    cached_output_buffer_size_ = 0;

    VABufferID output_buffer_id;
    if (!vaapi_wrapper_->CreateCodedBuffer(max_coded_buffer_size,
                                           &output_buffer_id)) {
      VLOG(1) << "Failed to create VA buffer for encoding output";
      notify_error_cb_.Run(video_frame_id, PLATFORM_FAILURE);
      return;
    }
    cached_output_buffer_size_ = max_coded_buffer_size;
    cached_output_buffer_id_ = output_buffer_id;
  }

  if (!jpeg_encoder_->Encode(input_size, request->quality, va_surface_id,
                             cached_output_buffer_id_)) {
    VLOG(1) << "Encode JPEG failed";
    notify_error_cb_.Run(video_frame_id, PLATFORM_FAILURE);
    return;
  }

  // Get the encoded output. DownloadFromCodedBuffer() is a blocking call. It
  // would wait until encoding is finished.
  size_t encoded_size = 0;
  if (!vaapi_wrapper_->DownloadFromCodedBuffer(
          cached_output_buffer_id_, va_surface_id,
          static_cast<uint8_t*>(request->shm->memory()), request->shm->size(),
          &encoded_size)) {
    VLOG(1) << "Failed to retrieve output image from VA coded buffer";
    notify_error_cb_.Run(video_frame_id, PLATFORM_FAILURE);
  }

  video_frame_ready_cb_.Run(request->video_frame->unique_id(), encoded_size);
}

VaapiJpegEncodeAccelerator::VaapiJpegEncodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(io_task_runner)),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

VaapiJpegEncodeAccelerator::~VaapiJpegEncodeAccelerator() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Destroying VaapiJpegEncodeAccelerator";

  weak_this_factory_.InvalidateWeakPtrs();
  encoder_task_runner_->DeleteSoon(FROM_HERE, std::move(encoder_));
}

void VaapiJpegEncodeAccelerator::NotifyError(int video_frame_id,
                                             Status status) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DLOG(ERROR) << "Notifying error: " << status;
  DCHECK(client_);
  client_->NotifyError(video_frame_id, status);
}

void VaapiJpegEncodeAccelerator::VideoFrameReady(int video_frame_id,
                                                 size_t encoded_picture_size) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  ReportToUMA(VAJEAEncoderResult::VAAPI_SUCCESS);

  client_->VideoFrameReady(video_frame_id, encoded_picture_size);
}

JpegEncodeAccelerator::Status VaapiJpegEncodeAccelerator::Initialize(
    JpegEncodeAccelerator::Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!VaapiWrapper::IsJpegEncodeSupported()) {
    return HW_JPEG_ENCODE_NOT_SUPPORTED;
  }

  client_ = client;
  scoped_refptr<VaapiWrapper> vaapi_wrapper = VaapiWrapper::Create(
      VaapiWrapper::kEncode, VAProfileJPEGBaseline,
      base::Bind(&ReportToUMA, VAJEAEncoderResult::VAAPI_ERROR));

  if (!vaapi_wrapper) {
    VLOG(1) << "Failed initializing VAAPI";
    return PLATFORM_FAILURE;
  }

  encoder_task_runner_ = base::CreateSingleThreadTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING});
  if (!encoder_task_runner_) {
    VLOG(1) << "Failed to create encoder task runner.";
    return THREAD_CREATION_FAILED;
  }

  encoder_ = std::make_unique<Encoder>(
      std::move(vaapi_wrapper),
      BindToCurrentLoop(base::BindRepeating(
          &VaapiJpegEncodeAccelerator::VideoFrameReady, weak_this_)),
      BindToCurrentLoop(base::BindRepeating(
          &VaapiJpegEncodeAccelerator::NotifyError, weak_this_)));

  return ENCODE_OK;
}

size_t VaapiJpegEncodeAccelerator::GetMaxCodedBufferSize(
    const gfx::Size& picture_size) {
  return VaapiJpegEncoder::GetMaxCodedBufferSize(picture_size);
}

void VaapiJpegEncodeAccelerator::Encode(
    scoped_refptr<media::VideoFrame> video_frame,
    int quality,
    const BitstreamBuffer& bitstream_buffer) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  int video_frame_id = video_frame->unique_id();
  TRACE_EVENT1("jpeg", "Encode", "input_id", video_frame_id);

  // TODO(shenghao): support other YUV formats.
  if (video_frame->format() != VideoPixelFormat::PIXEL_FORMAT_I420) {
    VLOG(1) << "Unsupported input format: " << video_frame->format();
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&VaapiJpegEncodeAccelerator::NotifyError,
                              weak_this_, video_frame_id, INVALID_ARGUMENT));
    return;
  }

  // SharedMemoryRegion will take ownership of the |bitstream_buffer.handle()|.
  auto shm = std::make_unique<SharedMemoryRegion>(bitstream_buffer, false);
  if (!shm->Map()) {
    VLOG(1) << "Failed to map output buffer";
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VaapiJpegEncodeAccelerator::NotifyError, weak_this_,
                   video_frame_id, INACCESSIBLE_OUTPUT_BUFFER));
    return;
  }

  auto request = std::make_unique<EncodeRequest>(std::move(video_frame),
                                                 std::move(shm), quality);
  encoder_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VaapiJpegEncodeAccelerator::Encoder::EncodeTask,
                 base::Unretained(encoder_.get()), base::Passed(&request)));
}

}  // namespace media
