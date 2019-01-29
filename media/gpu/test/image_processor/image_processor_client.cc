// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/image_processor/image_processor_client.h"

#include <utility>

#include "base/bind.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/image_processor_factory.h"
#include "media/gpu/test/image.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
namespace test {

namespace {
// TODO(crbug.com/917951): Move these functions to video_frame_helpers.h

// Copy |src_frame| into a new VideoFrame with |dst_layout|. The created
// VideoFrame's content is the same as |src_frame|. Returns nullptr on failure.
scoped_refptr<VideoFrame> CloneVideoFrameWithLayout(
    const VideoFrame* const src_frame,
    const VideoFrameLayout& dst_layout) {
  LOG_ASSERT(src_frame->IsMappable());
  LOG_ASSERT(src_frame->format() == dst_layout.format());
  // Create VideoFrame, which allocates and owns data.
  auto dst_frame = VideoFrame::CreateFrameWithLayout(
      dst_layout, src_frame->visible_rect(), src_frame->natural_size(),
      src_frame->timestamp(), false /* zero_initialize_memory*/);
  if (!dst_frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  // Copy every plane's content from |src_frame| to |dst_frame|.
  const size_t num_planes = VideoFrame::NumPlanes(dst_layout.format());
  LOG_ASSERT(dst_layout.planes().size() == num_planes);
  LOG_ASSERT(src_frame->layout().planes().size() == num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    libyuv::CopyPlane(
        src_frame->data(i), src_frame->layout().planes()[i].stride,
        dst_frame->data(i), dst_frame->layout().planes()[i].stride,
        VideoFrame::Columns(i, dst_frame->format(),
                            dst_frame->natural_size().width()),
        VideoFrame::Rows(i, dst_frame->format(),
                         dst_frame->natural_size().height()));
  }

  return dst_frame;
}

}  // namespace

// static
std::unique_ptr<ImageProcessorClient> ImageProcessorClient::Create(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers,
    bool store_processed_video_frames) {
  auto ip_client =
      base::WrapUnique(new ImageProcessorClient(store_processed_video_frames));
  if (!ip_client->CreateImageProcessor(input_config, output_config,
                                       num_buffers)) {
    LOG(ERROR) << "Failed to create ImageProcessor";
    return nullptr;
  }
  return ip_client;
}

ImageProcessorClient::ImageProcessorClient(bool store_processed_video_frames)
    : image_processor_client_thread_("ImageProcessorClientThread"),
      store_processed_video_frames_(store_processed_video_frames),
      output_cv_(&output_lock_),
      num_processed_frames_(0),
      image_processor_error_count_(0) {
  CHECK(image_processor_client_thread_.Start());
  DETACH_FROM_THREAD(image_processor_client_thread_checker_);
}

ImageProcessorClient::~ImageProcessorClient() {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  CHECK(image_processor_client_thread_.IsRunning());
  // Destroys |image_processor_| on |image_processor_client_thread_|.
  image_processor_client_thread_.task_runner()->DeleteSoon(
      FROM_HERE, image_processor_.release());
  image_processor_client_thread_.Stop();
}

bool ImageProcessorClient::CreateImageProcessor(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers) {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  // base::Unretained(this) and base::ConstRef() are safe here because |this|,
  // |input_config| and |output_config| must outlive because this task is
  // blocking.
  image_processor_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessorClient::CreateImageProcessorTask,
                     base::Unretained(this), base::ConstRef(input_config),
                     base::ConstRef(output_config), num_buffers, &done));
  done.Wait();
  if (!image_processor_) {
    LOG(ERROR) << "Failed to create ImageProcessor";
    return false;
  }
  return true;
}

void ImageProcessorClient::CreateImageProcessorTask(
    const ImageProcessor::PortConfig& input_config,
    const ImageProcessor::PortConfig& output_config,
    size_t num_buffers,
    base::WaitableEvent* done) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  // base::Unretained(this) for ErrorCB is safe here because the callback is
  // executed on |image_processor_client_thread_| which is owned by this class.
  image_processor_ = ImageProcessorFactory::Create(
      input_config, output_config, {ImageProcessor::OutputMode::IMPORT},
      num_buffers,
      base::BindRepeating(&ImageProcessorClient::NotifyError,
                          base::Unretained(this)));
  done->Signal();
}

scoped_refptr<VideoFrame> ImageProcessorClient::CreateInputFrame(
    const Image& input_image) const {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  LOG_ASSERT(image_processor_);
  LOG_ASSERT(input_image.IsLoaded());
  LOG_ASSERT(input_image.DataSize() ==
             VideoFrame::AllocationSize(input_image.PixelFormat(),
                                        input_image.Size()));

  const auto format = input_image.PixelFormat();
  const auto visible_size = input_image.Size();

  // Create planes for layout. We cannot use WrapExternalData() because it
  // calls GetDefaultLayout() and it supports only a few pixel formats.
  const size_t num_planes = VideoFrame::NumPlanes(format);
  std::vector<VideoFrameLayout::Plane> planes(num_planes);
  const auto strides = VideoFrame::ComputeStrides(format, visible_size);
  size_t offset = 0;
  for (size_t i = 0; i < num_planes; ++i) {
    planes[i].stride = strides[i];
    planes[i].offset = offset;
    offset += VideoFrame::PlaneSize(format, i, visible_size).GetArea();
  }

  auto layout = VideoFrameLayout::CreateWithPlanes(
      format, visible_size, std::move(planes), {input_image.DataSize()});
  if (!layout) {
    LOG(ERROR) << "Failed to create VideoFrameLayout";
    return nullptr;
  }

  auto frame = VideoFrame::WrapExternalDataWithLayout(
      *layout, gfx::Rect(visible_size), visible_size, input_image.Data(),
      input_image.DataSize(), base::TimeDelta());
  if (!frame) {
    LOG(ERROR) << "Failed to create VideoFrame";
    return nullptr;
  }

  const auto& input_layout = image_processor_->input_layout();
  if (VideoFrame::IsStorageTypeMappable(
          image_processor_->input_storage_type())) {
    return CloneVideoFrameWithLayout(frame.get(), input_layout);
  } else {
#if defined(OS_CHROMEOS)
    LOG_ASSERT(image_processor_->input_storage_type() ==
               VideoFrame::STORAGE_DMABUFS);
#endif
    // TODO(crbug.com/917951): Support Dmabuf.
    NOTIMPLEMENTED();
    return nullptr;
  }
}

scoped_refptr<VideoFrame> ImageProcessorClient::CreateOutputFrame(
    const Image& output_image) const {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  LOG_ASSERT(output_image.IsMetadataLoaded());
  LOG_ASSERT(image_processor_);

  const auto& output_layout = image_processor_->output_layout();
  if (VideoFrame::IsStorageTypeMappable(
          image_processor_->input_storage_type())) {
    return VideoFrame::CreateFrameWithLayout(
        output_layout, gfx::Rect(output_image.Size()), output_image.Size(),
        base::TimeDelta(), false /* zero_initialize_memory*/);
  } else {
#if defined(OS_CHROMEOS)
    LOG_ASSERT(image_processor_->input_storage_type() ==
               VideoFrame::STORAGE_DMABUFS);
#endif
    // TODO(crbug.com/917951): Support Dmabuf.
    NOTIMPLEMENTED();
    return nullptr;
  }
}

void ImageProcessorClient::FrameReady(scoped_refptr<VideoFrame> frame) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);

  base::AutoLock auto_lock_(output_lock_);
  if (store_processed_video_frames_)
    output_video_frames_.push_back(std::move(frame));
  num_processed_frames_++;
  output_cv_.Signal();
}

bool ImageProcessorClient::WaitUntilNumImageProcessed(
    size_t num_processed,
    base::TimeDelta max_wait) {
  base::TimeDelta time_waiting;
  // NOTE: Acquire lock here does not matter, because
  // base::ConditionVariable::TimedWait() unlocks output_lock_ at the start and
  // locks again at the end.
  base::AutoLock auto_lock_(output_lock_);
  while (time_waiting < max_wait) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    output_cv_.TimedWait(max_wait);
    time_waiting += base::TimeTicks::Now() - start_time;

    if (num_processed_frames_ >= num_processed)
      return true;
  }
  return false;
}

size_t ImageProcessorClient::GetNumOfProcessedImages() const {
  base::AutoLock auto_lock_(output_lock_);
  return num_processed_frames_;
}

std::vector<scoped_refptr<VideoFrame>>
ImageProcessorClient::GetProcessedImages() const {
  LOG_ASSERT(store_processed_video_frames_);
  base::AutoLock auto_lock_(output_lock_);
  return output_video_frames_;
}

size_t ImageProcessorClient::GetErrorCount() const {
  base::AutoLock auto_lock_(output_lock_);
  return image_processor_error_count_;
}

void ImageProcessorClient::NotifyError() {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  base::AutoLock auto_lock_(output_lock_);
  image_processor_error_count_++;
}

void ImageProcessorClient::Process(const Image& input_image,
                                   const Image& output_image) {
  DCHECK_CALLED_ON_VALID_THREAD(test_main_thread_checker_);
  auto input_frame = CreateInputFrame(input_image);
  ASSERT_TRUE(input_frame);
  auto output_frame = CreateOutputFrame(output_image);
  ASSERT_TRUE(output_frame);
  image_processor_client_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageProcessorClient::ProcessTask, base::Unretained(this),
                     std::move(input_frame), std::move(output_frame)));
}

void ImageProcessorClient::ProcessTask(scoped_refptr<VideoFrame> input_frame,
                                       scoped_refptr<VideoFrame> output_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(image_processor_client_thread_checker_);
  // base::Unretained(this) and base::ConstRef() for FrameReadyCB is safe here
  // because the callback is executed on |image_processor_client_thread_| which
  // is owned by this class.
  image_processor_->Process(
      std::move(input_frame), std::move(output_frame),
      BindToCurrentLoop(base::BindOnce(&ImageProcessorClient::FrameReady,
                                       base::Unretained(this))));
}

}  // namespace test
}  // namespace media
