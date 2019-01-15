// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_LIBYUV_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_LIBYUV_IMAGE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "media/gpu/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// A software image processor which uses libyuv to perform format conversion.
// It expects input VideoFrame is mapped into CPU space, and output VideoFrame
// is allocated in user space.
class MEDIA_GPU_EXPORT LibYUVImageProcessor : public ImageProcessor {
 public:
  // ImageProcessor override
  ~LibYUVImageProcessor() override;
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  bool Process(scoped_refptr<VideoFrame> frame,
               int output_buffer_index,
               std::vector<base::ScopedFD> output_dmabuf_fds,
               FrameReadyCB cb) override;
#endif
  bool Process(scoped_refptr<VideoFrame> input_frame,
               scoped_refptr<VideoFrame> output_frame,
               FrameReadyCB cb) override;
  bool Reset() override;

  // Factory method to create LibYUVImageProcessor to convert video format
  // specified in input_config and output_config. Provided |error_cb| will be
  // posted to the same thread Create() is called if an error occurs after
  // initialization.
  // Returns nullptr if it fails to create LibYUVImageProcessor.
  static std::unique_ptr<LibYUVImageProcessor> Create(
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      const ImageProcessor::OutputMode output_mode,
      ErrorCB error_cb);

 private:
  LibYUVImageProcessor(const VideoFrameLayout& input_layout,
                       const gfx::Size& input_visible_size,
                       VideoFrame::StorageType input_storage_type,
                       const VideoFrameLayout& output_layout,
                       const gfx::Size& output_visible_size,
                       VideoFrame::StorageType output_storage_type,
                       OutputMode output_mode,
                       ErrorCB error_cb);

  void ProcessTask(scoped_refptr<VideoFrame> input_frame,
                   scoped_refptr<VideoFrame> output_frame,
                   FrameReadyCB cb);

  // A processed frame is ready.
  void FrameReady(FrameReadyCB cb, scoped_refptr<VideoFrame> frame);

  // Posts error on |client_task_runner_| thread. This must be called in a
  // thread |client_task_runner_| doesn't belong to.
  void NotifyError();
  // Invokes |error_cb_|. This must be called in |client_task_runner_|'s thread.
  void NotifyErrorOnClientThread();

  static bool IsFormatSupported(VideoPixelFormat input_format,
                                VideoPixelFormat output_format);

  const gfx::Rect input_visible_rect_;
  const gfx::Rect output_visible_rect_;

  // Error callback to the client.
  ErrorCB error_cb_;

  // A task runner belongs to a thread where LibYUVImageProcessor is created.
  const scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  // Thread to process frame format conversion.
  base::Thread process_thread_;

  // CancelableTaskTracker for ProcessTask().
  // Because ProcessTask is posted from |client_task_runner_|'s thread to
  // another sequence, |process_thread_|, it is unsafe to cancel the posted task
  // from |client_task_runner_|'s thread using CancelableCallback and WeakPtr
  // binding. CancelableTaskTracker is designed to deal with this scenario.
  base::CancelableTaskTracker process_task_tracker_;

  // Checker for the thread |client_task_runner_| belongs to.
  THREAD_CHECKER(thread_checker_);

  // Emits weak pointer to |this| for tasks from |processor_thread_| to the
  // thread that creates LibYUVImageProcessor. So the tasks are cancelled if
  // |this| is invalidated, which leads to avoid calling FrameReadyCB and
  // ErrorCB of the invalidated client (e.g. V4L2VideoEncodeAccelerator).
  // On the other hand, since |process_thread_| is the member of this class, it
  // is guaranteed this instance is alive when a task on the thread is executed.
  // Therefore, base::Unretained(this) is safe for |processor_thread_| tasks
  // posted from |client_task_runner_|'s thread.
  // NOTE: |weak_this_| must always be dereferenced and invalidated on the
  // thread that creates LibYUVImageProcessor.
  base::WeakPtr<LibYUVImageProcessor> weak_this_;

  // The WeakPtrFactory for |weak_this_|.
  base::WeakPtrFactory<LibYUVImageProcessor> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(LibYUVImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_LIBYUV_IMAGE_PROCESSOR_H_
