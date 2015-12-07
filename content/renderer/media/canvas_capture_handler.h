// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CANVAS_CAPTURE_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_CANVAS_CAPTURE_HANDLER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/video_capturer_source.h"
#include "media/base/video_frame_pool.h"
#include "third_party/WebKit/public/platform/WebCanvasCaptureHandler.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebSkImage.h"
#include "third_party/skia/include/core/SkImage.h"

namespace content {

class CONTENT_EXPORT CanvasCaptureHandler final
    : public NON_EXPORTED_BASE(blink::WebCanvasCaptureHandler) {
 public:
  CanvasCaptureHandler(const blink::WebSize& size,
                       double frame_rate,
                       blink::WebMediaStream* stream);

  // blink::WebCanvasCaptureHandler Implementation.
  void sendNewFrame(const blink::WebSkImage& image) override;
  bool needsNewFrame() const override;

  // Functions called by media::VideoCapturerSource implementation.
  void StartVideoCapture(
      const media::VideoCaptureParams& params,
      const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
          new_frame_callback,
      const media::VideoCapturerSource::RunningCallback& running_callback);
  void StopVideoCapture();
  blink::WebSize GetSourceSize() const { return size_; }

 private:
  ~CanvasCaptureHandler() override;

  void CreateNewFrame(const blink::WebSkImage& image);

  // Implementation VideoCapturerSource that is owned by blink and delegates
  // the Start/Stop calls to CanvasCaptureHandler.
  class VideoCapturerSource;
  // Object that does all the work of running |new_frame_callback_|.
  // Destroyed on |io_task_runner_| after the class is destroyed.
  class CanvasCaptureHandlerDelegate;

  media::VideoCaptureFormat capture_format_;
  bool ask_for_new_frame_;

  const blink::WebSize size_;
  gfx::Size last_size;
  std::vector<uint8> temp_data_;
  size_t row_bytes_;
  SkImageInfo image_info_;
  media::VideoFramePool frame_pool_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_ptr<CanvasCaptureHandlerDelegate> delegate_;
  // Bound to the main render thread.
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<CanvasCaptureHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CANVAS_CAPTURE_HANDLER_H_
