// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include "base/callback.h"
#include "base/message_loop/message_loop_proxy.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/video/capture/video_capture.h"

namespace content {

class VideoCaptureHandle;

// VideoCapturerDelegate is a delegate used by MediaStreamVideoCapturerSource
// for local video capturer. It uses VideoCaptureImplManager to start / stop
// and receive I420 frames from Chrome's video capture implementation.
class CONTENT_EXPORT VideoCapturerDelegate
    : public media::VideoCapture::EventHandler,
      public base::RefCountedThreadSafe<VideoCapturerDelegate> {
 public:
  typedef base::Callback<void(const scoped_refptr<media::VideoFrame>&)>
      NewFrameCallback;
  typedef base::Callback<void(bool running)> StartedCallback;
  typedef base::Callback<void(const media::VideoCaptureFormats& formats)>
      SupportedFormatsCallback;

  explicit VideoCapturerDelegate(
      const StreamDeviceInfo& device_info);

  // Collects the formats that can currently be used.
  // |max_requested_height| and |max_requested_width| is used by Tab and Screen
  // capture to decide what resolution to generate.
  // |callback| is triggered when the formats have been collected.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      const SupportedFormatsCallback& callback);

  // Starts deliver frames using the resolution in |params|.
  // |new_frame_callback| is triggered when a new video frame is available.
  // |started_callback| is triggered before the first video frame is received
  // or if the underlying video capturer fails to start.
  virtual void StartDeliver(
      const media::VideoCaptureParams& params,
      const NewFrameCallback& new_frame_callback,
      const StartedCallback& started_callback);

  // Stops deliver frames and clears all callbacks including the
  // SupportedFormatsCallback callback.
  virtual void StopDeliver();

 protected:
  // media::VideoCapture::EventHandler implementation.
  // These functions are called on the IO thread (same as where
  // |capture_engine_| runs).
  virtual void OnStarted(media::VideoCapture* capture) OVERRIDE;
  virtual void OnStopped(media::VideoCapture* capture) OVERRIDE;
  virtual void OnPaused(media::VideoCapture* capture) OVERRIDE;
  virtual void OnError(media::VideoCapture* capture, int error_code) OVERRIDE;
  virtual void OnRemoved(media::VideoCapture* capture) OVERRIDE;
  virtual void OnFrameReady(
      media::VideoCapture* capture,
      const scoped_refptr<media::VideoFrame>& frame) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<VideoCapturerDelegate>;
  friend class MockVideoCapturerDelegate;

  virtual ~VideoCapturerDelegate();

  void OnFrameReadyOnRenderThread(
      media::VideoCapture* capture,
      const scoped_refptr<media::VideoFrame>& frame);
  void OnErrorOnRenderThread(media::VideoCapture* capture);
  void OnDeviceFormatsInUseReceived(const media::VideoCaptureFormats& formats);
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& formats);

  // The id identifies which video capture device is used for this video
  // capture session.
  media::VideoCaptureSessionId session_id_;
  scoped_ptr<VideoCaptureHandle> capture_engine_;

  bool is_screen_cast_;

  // Accessed on the thread where StartDeliver is called.
  bool got_first_frame_;

  // |new_frame_callback_| is provided to this class in StartDeliver and must be
  // valid until StopDeliver is called.
  NewFrameCallback new_frame_callback_;
  // |started_callback| is provided to this class in StartDeliver and must be
  // valid until StopDeliver is called.
  StartedCallback started_callback_;
  // Message loop of the caller of StartDeliver.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  SupportedFormatsCallback source_formats_callback_;

  DISALLOW_COPY_AND_ASSIGN(VideoCapturerDelegate);
};

class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource {
 public:
  MediaStreamVideoCapturerSource(
      const StreamDeviceInfo& device_info,
      const SourceStoppedCallback& stop_callback,
      const scoped_refptr<VideoCapturerDelegate>& delegate,
      MediaStreamDependencyFactory* factory);

  virtual ~MediaStreamVideoCapturerSource();

 protected:
  // Implements MediaStreamVideoSource.
  virtual void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height) OVERRIDE;

  virtual void StartSourceImpl(
      const media::VideoCaptureParams& params) OVERRIDE;

  virtual void StopSourceImpl() OVERRIDE;

 private:
  // The delegate that provides video frames.
  scoped_refptr<VideoCapturerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
