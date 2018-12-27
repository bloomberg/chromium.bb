// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/libyuv_image_processor.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/gpu/macros.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

namespace media {

LibYUVImageProcessor::LibYUVImageProcessor(
    const VideoFrameLayout& input_layout,
    const gfx::Size& input_visible_size,
    VideoFrame::StorageType input_storage_type,
    const VideoFrameLayout& output_layout,
    const gfx::Size& output_visible_size,
    VideoFrame::StorageType output_storage_type,
    ErrorCB error_cb)
    : input_layout_(input_layout),
      input_visible_rect_(input_visible_size),
      input_storage_type_(input_storage_type),
      output_layout_(output_layout),
      output_visible_rect_(output_visible_size),
      output_storage_type_(output_storage_type),
      error_cb_(error_cb),
      client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      process_thread_("LibYUVImageProcessorThread"),
      weak_this_factory_(this) {
  weak_this_ = weak_this_factory_.GetWeakPtr();
}

LibYUVImageProcessor::~LibYUVImageProcessor() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_this_factory_.InvalidateWeakPtrs();
  process_thread_.Stop();
}

// static
std::unique_ptr<LibYUVImageProcessor> LibYUVImageProcessor::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    const ImageProcessor::OutputMode output_mode,
    ErrorCB error_cb) {
  VLOGF(2);

  if (!IsFormatSupported(input_config.layout.format(),
                         output_config.layout.format())) {
    VLOGF(2) << "Conversion from " << input_config.layout.format() << " to "
             << output_config.layout.format() << " is not supported";
    return nullptr;
  }

  // LibYUVImageProcessor supports only memory-based video frame for input.
  VideoFrame::StorageType input_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto input_type : input_config.preferred_storage_types) {
    if (VideoFrame::IsStorageTypeMappable(input_type)) {
      input_storage_type = input_type;
      break;
    }
  }
  if (input_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported input storage type";
    return nullptr;
  }

  // LibYUVImageProcessor supports only memory-based video frame for output.
  VideoFrame::StorageType output_storage_type = VideoFrame::STORAGE_UNKNOWN;
  for (auto output_type : output_config.preferred_storage_types) {
    if (VideoFrame::IsStorageTypeMappable(output_type)) {
      output_storage_type = output_type;
      break;
    }
  }
  if (output_storage_type == VideoFrame::STORAGE_UNKNOWN) {
    VLOGF(2) << "Unsupported output storage type";
    return nullptr;
  }

  if (output_mode != OutputMode::IMPORT) {
    VLOGF(2) << "Only support OutputMode::IMPORT";
    return nullptr;
  }

  auto processor = base::WrapUnique(new LibYUVImageProcessor(
      input_config.layout, input_config.visible_size, input_storage_type,
      output_config.layout, output_config.visible_size, output_storage_type,
      std::move(error_cb)));
  if (!processor->process_thread_.Start()) {
    VLOGF(1) << "Failed to start processing thread";
    return nullptr;
  }

  VLOGF(2) << "LibYUVImageProcessor created for converting from "
           << input_config.layout << " to " << output_config.layout;
  return processor;
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
bool LibYUVImageProcessor::Process(
    scoped_refptr<VideoFrame> frame,
    int output_buffer_index,
    std::vector<base::ScopedFD> output_dmabuf_fds,
    FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return false;
}
#endif

bool LibYUVImageProcessor::Process(scoped_refptr<VideoFrame> input_frame,
                                   scoped_refptr<VideoFrame> output_frame,
                                   FrameReadyCB cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOGF(4);
  DCHECK_EQ(input_frame->layout().format(), input_layout_.format());
  DCHECK(input_frame->layout().coded_size() == input_layout_.coded_size());
  DCHECK_EQ(output_frame->layout().format(), output_layout_.format());
  DCHECK(output_frame->layout().coded_size() == output_layout_.coded_size());
  DCHECK(VideoFrame::IsStorageTypeMappable(input_frame->storage_type()));
  DCHECK(VideoFrame::IsStorageTypeMappable(output_frame->storage_type()));
  process_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&LibYUVImageProcessor::ProcessTask,
                                base::Unretained(this), std::move(input_frame),
                                std::move(output_frame), std::move(cb)));
  return true;
}

void LibYUVImageProcessor::ProcessTask(scoped_refptr<VideoFrame> input_frame,
                                       scoped_refptr<VideoFrame> output_frame,
                                       FrameReadyCB cb) {
  DCHECK(process_thread_.task_runner()->BelongsToCurrentThread());
  DVLOGF(4);
  int result = libyuv::I420ToNV12(input_frame->data(VideoFrame::kYPlane),
                                  input_frame->stride(VideoFrame::kYPlane),
                                  input_frame->data(VideoFrame::kUPlane),
                                  input_frame->stride(VideoFrame::kUPlane),
                                  input_frame->data(VideoFrame::kVPlane),
                                  input_frame->stride(VideoFrame::kVPlane),
                                  output_frame->data(VideoFrame::kYPlane),
                                  output_frame->stride(VideoFrame::kYPlane),
                                  output_frame->data(VideoFrame::kUVPlane),
                                  output_frame->stride(VideoFrame::kUVPlane),
                                  output_frame->visible_rect().width(),
                                  output_frame->visible_rect().height());
  if (result != 0) {
    VLOGF(1) << "libyuv::I420ToNV12 returns non-zero code: " << result;
    NotifyError();
  }
  client_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LibYUVImageProcessor::FrameReady, weak_this_,
                                std::move(cb), std::move(output_frame)));
}

void LibYUVImageProcessor::FrameReady(FrameReadyCB cb,
                                      scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(cb).Run(std::move(frame));
}

bool LibYUVImageProcessor::Reset() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_this_factory_.InvalidateWeakPtrs();
  process_thread_.Stop();

  weak_this_ = weak_this_factory_.GetWeakPtr();
  if (!process_thread_.Start()) {
    VLOGF(1) << "process_thread_ failed to start";
    return false;
  }
  return true;
}

gfx::Size LibYUVImageProcessor::input_allocated_size() const {
  return input_layout_.coded_size();
}

gfx::Size LibYUVImageProcessor::output_allocated_size() const {
  return output_layout_.coded_size();
}

VideoFrame::StorageType LibYUVImageProcessor::input_storage_type() const {
  return input_storage_type_;
}

VideoFrame::StorageType LibYUVImageProcessor::output_storage_type() const {
  return output_storage_type_;
}

ImageProcessor::OutputMode LibYUVImageProcessor::output_mode() const {
  return OutputMode::IMPORT;
}

void LibYUVImageProcessor::NotifyError() {
  VLOGF(1);
  DCHECK(!client_task_runner_->BelongsToCurrentThread());
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LibYUVImageProcessor::NotifyErrorOnClientThread,
                     weak_this_));
}

void LibYUVImageProcessor::NotifyErrorOnClientThread() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  error_cb_.Run();
}

// static
bool LibYUVImageProcessor::IsFormatSupported(VideoPixelFormat input_format,
                                             VideoPixelFormat output_format) {
  if (input_format == PIXEL_FORMAT_I420) {
    if (output_format == PIXEL_FORMAT_NV12) {
      return true;
    } else {
      VLOGF(2) << "Unsupported output format: " << output_format
               << " for converting input format: " << input_format;
      return false;
    }
  } else {
    VLOGF(2) << "Unsupported input format: " << input_format;
    return false;
  }
}

}  // namespace media
