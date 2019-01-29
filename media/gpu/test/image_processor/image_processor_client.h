// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_IMAGE_PROCESSOR_IMAGE_PROCESSOR_CLIENT_H_
#define MEDIA_GPU_TEST_IMAGE_PROCESSOR_IMAGE_PROCESSOR_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/gpu/image_processor.h"

namespace base {

class WaitableEvent;

}  // namespace base

namespace media {

class VideoFrame;

namespace test {

class Image;

// ImageProcessorClient is a client of ImageProcessor for testing purpose.
// All the public functions must be called on the same thread, usually the test
// main thread.
class ImageProcessorClient {
 public:
  // |store_processed_video_frames| is whether ImageProcessorClient will store
  // VideoFrame in FrameReady().
  // TODO(crbug.com/917951): Get VideoFrameProcessor and does not store
  // VideoFrame.
  static std::unique_ptr<ImageProcessorClient> Create(
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      size_t num_buffers,
      bool store_processed_video_frames = true);

  // Destruct |image_processor_| if it is created.
  ~ImageProcessorClient();

  // Process |input_frame| and |output_frame| with |image_processor_|.
  // Processing is done asynchronously, the WaitUntilNumImageProcessed()
  // function can be used to wait for the results.
  void Process(const Image& input_image, const Image& output_image);

  // TODO(crbug.com/917951): Add Reset() when we test Reset() test case.

  // Wait until |num_processed| frames are processed.
  bool WaitUntilNumImageProcessed(
      size_t num_processed,
      base::TimeDelta max_wait = base::TimeDelta::FromSeconds(5));

  // Get the number of processed VideoFrames.
  size_t GetNumOfProcessedImages() const;

  // Get processed VideoFrames.
  // This always returns empty unless |store_processed_video_frames_|
  // TODO(hiroh): Remove this function and instead check md5sum and order
  // interactively inside of ImageProcessorClient by passing some
  // VideoFrameProcessor interface class.
  std::vector<scoped_refptr<VideoFrame>> GetProcessedImages() const;

  // Return whether |image_processor_| invokes ImageProcessor::ErrorCB.
  size_t GetErrorCount() const;

 private:
  explicit ImageProcessorClient(bool store_processed_video_frames);

  // Create ImageProcessor with |input_config|, |output_config| and
  // |num_buffers|.
  bool CreateImageProcessor(const ImageProcessor::PortConfig& input_config,
                            const ImageProcessor::PortConfig& output_config,
                            size_t num_buffers);

  // Create |image_processor_| on |my_thread_|.
  void CreateImageProcessorTask(const ImageProcessor::PortConfig& input_config,
                                const ImageProcessor::PortConfig& output_config,
                                size_t num_buffers,
                                base::WaitableEvent* done);

  // Call ImageProcessor::Process() on |my_thread_|.
  void ProcessTask(scoped_refptr<VideoFrame> input_frame,
                   scoped_refptr<VideoFrame> output_frame);

  // FrameReadyCB for ImageProcessor::Process().
  void FrameReady(scoped_refptr<VideoFrame> frame);
  // ErrorCB for ImageProcessor.
  void NotifyError();

  // These are test helper functions to create a VideoFrame from VideoImageInfo,
  // which will be input in Process().
  // Create a VideoFrame using the input layout required by |image_processor_|.
  scoped_refptr<VideoFrame> CreateInputFrame(const Image& input_image) const;
  // Create a VideoFrame using the output layout required by |image_processor_|.
  scoped_refptr<VideoFrame> CreateOutputFrame(const Image& output_image) const;

  std::unique_ptr<ImageProcessor> image_processor_;

  // The thread on which the |image_processor_| is created and destroyed. From
  // the specification of ImageProcessor, ImageProcessor::Process(),
  // FrameReady() and NotifyError() must be run on
  // |image_processor_client_thread_|.
  base::Thread image_processor_client_thread_;

  // If true, ImageProcessorClient stores processed VideoFrame in
  // |output_video_frames_|.
  const bool store_processed_video_frames_;

  mutable base::Lock output_lock_;
  // This is signaled in FrameReady().
  base::ConditionVariable output_cv_;
  // The number of processed VideoFrame.
  size_t num_processed_frames_ GUARDED_BY(output_lock_);
  // The collection of processed VideoFrame. It is stored in FrameReady() call
  // order.
  std::vector<scoped_refptr<VideoFrame>> output_video_frames_
      GUARDED_BY(output_lock_);
  // The number of times ImageProcessor::ErrorCB called.
  size_t image_processor_error_count_ GUARDED_BY(output_lock_);

  THREAD_CHECKER(image_processor_client_thread_checker_);
  THREAD_CHECKER(test_main_thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(ImageProcessorClient);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_IMAGE_PROCESSOR_IMAGE_PROCESSOR_CLIENT_H_
