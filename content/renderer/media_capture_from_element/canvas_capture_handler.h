// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAPTURE_FROM_ELEMENT_CANVAS_CAPTURE_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_CAPTURE_FROM_ELEMENT_CANVAS_CAPTURE_HANDLER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/video_frame_pool.h"
#include "media/capture/video_capturer_source.h"
#include "third_party/WebKit/public/platform/WebCanvasCaptureHandler.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class SkImage;

namespace content {

// CanvasCaptureHandler acts as the link between Blink side HTMLCanvasElement
// and Chrome side VideoCapturerSource. It is responsible for handling
// SkImage instances sent from the Blink side, convert them to
// media::VideoFrame and plug them to the MediaStreamTrack.
// CanvasCaptureHandler instance is owned by a blink::CanvasDrawListener which
// is owned by a CanvasCaptureMediaStreamTrack.
// All methods are called on the same thread as construction and destruction,
// i.e. the Main Render thread. Note that a CanvasCaptureHandlerDelegate is
// used to send back frames to |io_task_runner_|, i.e. IO thread.
class CONTENT_EXPORT CanvasCaptureHandler final
    : public blink::WebCanvasCaptureHandler {
 public:
  ~CanvasCaptureHandler() override;

  // Creates a CanvasCaptureHandler instance and updates UMA histogram.
  static std::unique_ptr<CanvasCaptureHandler> CreateCanvasCaptureHandler(
      const blink::WebSize& size,
      double frame_rate,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      blink::WebMediaStreamTrack* track);

  // blink::WebCanvasCaptureHandler implementation.
  void SendNewFrame(
      sk_sp<SkImage> image,
      blink::WebGraphicsContext3DProvider* context_provider) override;
  bool NeedsNewFrame() const override;

  // Functions called by media::VideoCapturerSource implementation.
  void StartVideoCapture(
      const media::VideoCaptureParams& params,
      const media::VideoCapturerSource::VideoCaptureDeliverFrameCB&
          new_frame_callback,
      const media::VideoCapturerSource::RunningCallback& running_callback);
  void RequestRefreshFrame();
  void StopVideoCapture();

 private:
  // A VideoCapturerSource instance is created, which is responsible for handing
  // stop&start callbacks back to CanvasCaptureHandler. That VideoCapturerSource
  // is then plugged into a MediaStreamTrack passed as |track|, and it is owned
  // by the Blink side MediaStreamSource.
  CanvasCaptureHandler(
      const blink::WebSize& size,
      double frame_rate,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      blink::WebMediaStreamTrack* track);

  // Helper functions to read pixel content.
  void ReadARGBPixelsSync(sk_sp<SkImage> image);
  void ReadARGBPixelsAsync(
      sk_sp<SkImage> image,
      blink::WebGraphicsContext3DProvider* context_provider);
  void ReadYUVPixelsAsync(
      sk_sp<SkImage> image,
      blink::WebGraphicsContext3DProvider* context_provider);
  void OnARGBPixelsReadAsync(sk_sp<SkImage> image,
                             scoped_refptr<media::VideoFrame> temp_argb_frame,
                             base::TimeTicks this_frame_ticks,
                             bool flip,
                             bool success);
  void OnYUVPixelsReadAsync(sk_sp<SkImage> image,
                            scoped_refptr<media::VideoFrame> yuv_frame,
                            base::TimeTicks this_frame_ticks,
                            bool success);

  scoped_refptr<media::VideoFrame> ConvertToYUVFrame(
      bool is_opaque,
      bool flip,
      const uint8_t* source_ptr,
      const gfx::Size& image_size,
      int stride,
      SkColorType source_color_type);
  void SendFrame(scoped_refptr<media::VideoFrame> video_frame,
                 base::TimeTicks this_frame_ticks);

  void AddVideoCapturerSourceToVideoTrack(
      std::unique_ptr<media::VideoCapturerSource> source,
      blink::WebMediaStreamTrack* web_track);

  // Object that does all the work of running |new_frame_callback_|.
  // Destroyed on |frame_callback_task_runner_| after the class is destroyed.
  class CanvasCaptureHandlerDelegate;

  media::VideoCaptureFormat capture_format_;
  bool ask_for_new_frame_;
  media::VideoFramePool frame_pool_;
  base::Optional<base::TimeTicks> first_frame_ticks_;
  scoped_refptr<media::VideoFrame> last_frame_;

  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  std::unique_ptr<CanvasCaptureHandlerDelegate> delegate_;

  // Bound to Main Render thread.
  base::ThreadChecker main_render_thread_checker_;
  base::WeakPtrFactory<CanvasCaptureHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasCaptureHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CAPTURE_FROM_ELEMENT_CANVAS_CAPTURE_HANDLER_H_
