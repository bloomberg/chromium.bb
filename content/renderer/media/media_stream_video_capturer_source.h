// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread_checker.h"
#include "content/common/media/video_capture.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/video_capturer_source.h"

namespace content {

// VideoCapturerDelegate is a delegate used by MediaStreamVideoCapturerSource
// for local video capturer. It uses VideoCaptureImplManager to start / stop
// and receive I420 frames from Chrome's video capture implementation.
//
// This is a render thread only object.

class CONTENT_EXPORT VideoCapturerDelegate : public media::VideoCapturerSource {
 public:
  explicit VideoCapturerDelegate(const StreamDeviceInfo& device_info);
  ~VideoCapturerDelegate() override;

  // VideoCaptureDelegate Implementation.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;

  void StartCapture(
      const media::VideoCaptureParams& params,
      const VideoCaptureDeliverFrameCB& new_frame_callback,
      scoped_refptr<base::SingleThreadTaskRunner> frame_callback_task_runner,
      const RunningCallback& running_callback) override;

  void StopCapture() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoCapturerSourceTest, Ended);
  friend class MockVideoCapturerDelegate;

  void OnStateUpdate(VideoCaptureState state);
  void OnDeviceFormatsInUseReceived(const media::VideoCaptureFormats& formats);
  void OnDeviceSupportedFormatsEnumerated(
      const media::VideoCaptureFormats& formats);

  // The id identifies which video capture device is used for this video
  // capture session.
  const media::VideoCaptureSessionId session_id_;
  base::Closure release_device_cb_;
  base::Closure stop_capture_cb_;

  const bool is_screen_cast_;

  // |running_callback| is provided to this class in StartCapture and must be
  // valid until StopCapture is called.
  RunningCallback running_callback_;

  VideoCaptureDeviceFormatsCB source_formats_callback_;

  // Bound to the render thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<VideoCapturerDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCapturerDelegate);
};

// Owned by WebMediaStreamSource in Blink as a representation of a video
// stream coming from a camera.
// This is a render thread only object. All methods must be called on the
// render thread.
class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource {
 public:
  MediaStreamVideoCapturerSource(
      const SourceStoppedCallback& stop_callback,
      scoped_ptr<media::VideoCapturerSource> delegate);
  virtual ~MediaStreamVideoCapturerSource();

  void SetDeviceInfo(const StreamDeviceInfo& device_info);

 protected:
  // Implements MediaStreamVideoSource.
  void GetCurrentSupportedFormats(
      int max_requested_width,
      int max_requested_height,
      double max_requested_frame_rate,
      const VideoCaptureDeviceFormatsCB& callback) override;

  void StartSourceImpl(
      const media::VideoCaptureFormat& format,
      const VideoCaptureDeliverFrameCB& frame_callback) override;

  void StopSourceImpl() override;

 private:
  void OnStarted(bool result);
  // The delegate that provides video frames.
  const scoped_ptr<media::VideoCapturerSource> delegate_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
