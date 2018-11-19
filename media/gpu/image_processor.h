// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_IMAGE_PROCESSOR_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoFrame;

// An image processor is used to convert from one image format to another (e.g.
// I420 to NV12) while optionally scaling. It is useful in situations where
// a given video hardware (e.g. decoder or encoder) accepts or produces data
// in a format different from what the rest of the pipeline expects.
//
// This class exposes the interface that an image processor should implement.
class ImageProcessor {
 public:

  // Returns input allocated size required by the processor to be fed with.
  virtual gfx::Size input_allocated_size() const = 0;

  // Returns output allocated size required by the processor.
  virtual gfx::Size output_allocated_size() const = 0;

  // Callback to be used to return the index of a processed image to the
  // client. After the client is done with the frame, call Process with the
  // index to return the output buffer to the image processor.
  using FrameReadyCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;

  // Called by client to process |frame|. The resulting processed frame will be
  // stored in |output_buffer_index| output buffer and notified via |cb|. The
  // processor will drop all its references to |frame| after it finishes
  // accessing it. If the input buffers are DMA-backed, the caller
  // should pass non-empty |output_dmabuf_fds| and the processed frame will be
  // stored in those buffers. If the number of |output_dmabuf_fds| is not
  // expected, this function will return false.
  virtual bool Process(scoped_refptr<VideoFrame> frame,
                       int output_buffer_index,
                       std::vector<base::ScopedFD> output_dmabuf_fds,
                       FrameReadyCB cb) = 0;

  // Reset all processing frames. After this method returns, no more callbacks
  // will be invoked. ImageProcessor is ready to process more frames.
  virtual bool Reset() = 0;

  virtual ~ImageProcessor() = default;
};

}  // namespace media

#endif  // MEDIA_GPU_IMAGE_PROCESSOR_H_
