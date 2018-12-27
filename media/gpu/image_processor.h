// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_IMAGE_PROCESSOR_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// An image processor is used to convert from one image format to another (e.g.
// I420 to NV12) while optionally scaling. It is useful in situations where
// a given video hardware (e.g. decoder or encoder) accepts or produces data
// in a format different from what the rest of the pipeline expects.
//
// This class exposes the interface that an image processor should implement.
// The threading model of ImageProcessor:
// Process(), Reset(), and callbacks: FrameReadyCB, ErrorCB, should be run in
// the same thread that creates ImageProcessor.
class ImageProcessor {
 public:
  // OutputMode is used as intermediate stage. The ultimate goal is to make
  // ImageProcessor's clients all use IMPORT output mode.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  enum class OutputMode {
    ALLOCATE,
    IMPORT
  };

  // Encapsulates ImageProcessor input / output configurations.
  struct PortConfig {
    PortConfig() = delete;
    PortConfig(
        const VideoFrameLayout& layout,
        const gfx::Size& visible_size,
        const std::vector<VideoFrame::StorageType>& preferred_storage_types);
    ~PortConfig();

    const VideoFrameLayout layout;
    const gfx::Size visible_size;
    const std::vector<VideoFrame::StorageType> preferred_storage_types;
  };

  // Callback to be used to return a processed image to the client.
  // FrameReadyCB shall be executed on the thread that creates ImageProcessor.
  // ImageProcessor has to bind its weak pointer to the task to execute
  // FrameReadyCB so that the task will not be called after ImageProcessor
  // instance is destructed. Note that ImageProcessor client instance should
  // have the same lifetime of or outlive ImageProcessor.
  using FrameReadyCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  // Callback to be used to notify client when ImageProcess encounters error.
  // It should be assigned in subclass's factory method.
  // ErrorCB shall be executed on the thread that creates ImageProcessor.
  // ImageProcessor has to bind its weak pointer to the task to execute ErrorCB
  // so that the task will not be called after ImageProcessor instance is
  // destructed. Note that ImageProcessor client instance should have the same
  // lifetime of or outlive ImageProcessor.
  using ErrorCB = base::RepeatingClosure;

  virtual ~ImageProcessor() = default;

  // Returns input layout of the processor.
  const VideoFrameLayout& input_layout() const { return input_layout_; }

  // Returns output layout of the processor.
  const VideoFrameLayout& output_layout() const { return output_layout_; }

  // Returns input storage type.
  VideoFrame::StorageType input_storage_type() const {
    return input_storage_type_;
  }

  // Returns output storage type.
  VideoFrame::StorageType output_storage_type() const {
    return output_storage_type_;
  }

  // Returns output mode.
  // TODO(crbug.com/907767): Remove it once ImageProcessor always works as
  // IMPORT mode for output.
  OutputMode output_mode() const { return output_mode_; }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Called by client to process |frame|. The resulting processed frame will be
  // stored in |output_buffer_index| output buffer and notified via |cb|. The
  // processor will drop all its references to |frame| after it finishes
  // accessing it. If the input buffers are DMA-backed, the caller
  // should pass non-empty |output_dmabuf_fds| and the processed frame will be
  // stored in those buffers. If the number of |output_dmabuf_fds| is not
  // expected, this function will return false.
  // Process() must be called on the same thread that creates ImageProcessor.
  //
  // Note: because base::ScopedFD is defined under OS_POXIS or OS_FUCHSIA, we
  // define this function under the same build config. It doesn't affect its
  // current users as they are all under the same build config.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  virtual bool Process(scoped_refptr<VideoFrame> frame,
                       int output_buffer_index,
                       std::vector<base::ScopedFD> output_dmabuf_fds,
                       FrameReadyCB cb) = 0;
#endif

  // Called by client to process |input_frame| and store in |output_frame|. This
  // can only be used when output mode is IMPORT. The processor will drop all
  // its references to |input_frame| and |output_frame| after it finishes
  // accessing it.
  // Process() must be called on the same thread that creates ImageProcessor.
  virtual bool Process(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) = 0;

  // Reset all processing frames. After this method returns, no more callbacks
  // will be invoked. ImageProcessor is ready to process more frames.
  // Reset() must be called on the same thread that creates ImageProcessor.
  virtual bool Reset() = 0;

 protected:
  ImageProcessor(const VideoFrameLayout& input_layout,
                 VideoFrame::StorageType input_storage_type,
                 const VideoFrameLayout& output_layout,
                 VideoFrame::StorageType output_storage_type,
                 OutputMode output_mode);

  // Stores input frame's layout and storage type.
  const VideoFrameLayout input_layout_;
  const VideoFrame::StorageType input_storage_type_;

  // Stores output frame's layout, storage type and output mode.
  // TODO(crbug.com/907767): Remove |output_mode_| once ImageProcessor always
  // works as IMPORT mode for output.
  const VideoFrameLayout output_layout_;
  const VideoFrame::StorageType output_storage_type_;
  const OutputMode output_mode_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_IMAGE_PROCESSOR_H_
