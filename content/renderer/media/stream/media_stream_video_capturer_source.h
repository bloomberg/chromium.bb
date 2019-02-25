// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
#define CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "media/capture/video_capture_types.h"
#include "third_party/blink/public/common/media/video_capture.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace media {
class VideoCapturerSource;
}  // namespace media

namespace content {

class RenderFrame;

// Representation of a video stream coming from a camera, owned by Blink as
// WebMediaStreamSource. Objects of this class are created and live on main
// Render thread. Objects can be constructed either by indicating a |device| to
// look for, or by plugging in a |source| constructed elsewhere.
class CONTENT_EXPORT MediaStreamVideoCapturerSource
    : public MediaStreamVideoSource {
 public:
  using DeviceCapturerFactoryCallback =
      base::RepeatingCallback<std::unique_ptr<media::VideoCapturerSource>(
          int session_id)>;
  MediaStreamVideoCapturerSource(
      const SourceStoppedCallback& stop_callback,
      std::unique_ptr<media::VideoCapturerSource> source);
  MediaStreamVideoCapturerSource(
      int render_frame_id,
      const SourceStoppedCallback& stop_callback,
      const blink::MediaStreamDevice& device,
      const media::VideoCaptureParams& capture_params,
      DeviceCapturerFactoryCallback device_capturer_factory_callback);
  ~MediaStreamVideoCapturerSource() override;

  void SetDeviceCapturerFactoryCallbackForTesting(
      DeviceCapturerFactoryCallback testing_factory_callback);

 private:
  friend class CanvasCaptureHandlerTest;
  friend class MediaStreamVideoCapturerSourceTest;
  friend class MediaStreamVideoCapturerSourceOldConstraintsTest;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoCapturerSourceTest, StartAndStop);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoCapturerSourceTest,
                           CaptureTimeAndMetadataPlumbing);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamVideoCapturerSourceTest, ChangeSource);

  // MediaStreamVideoSource overrides.
  void RequestRefreshFrame() override;
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override;
  void OnLog(const std::string& message) override;
  void OnHasConsumers(bool has_consumers) override;
  void OnCapturingLinkSecured(bool is_secure) override;
  void StartSourceImpl(
      const blink::VideoCaptureDeliverFrameCB& frame_callback) override;
  void StopSourceImpl() override;
  void StopSourceForRestartImpl() override;
  void RestartSourceImpl(const media::VideoCaptureFormat& new_format) override;
  base::Optional<media::VideoCaptureFormat> GetCurrentFormat() const override;
  base::Optional<media::VideoCaptureParams> GetCurrentCaptureParams()
      const override;
  void ChangeSourceImpl(const blink::MediaStreamDevice& new_device) override;

  // Method to bind as RunningCallback in VideoCapturerSource::StartCapture().
  void OnRunStateChanged(const media::VideoCaptureParams& new_capture_params,
                         bool is_running);

  const blink::mojom::MediaStreamDispatcherHostPtr&
  GetMediaStreamDispatcherHost(RenderFrame* render_frame);

  blink::mojom::MediaStreamDispatcherHostPtr dispatcher_host_;

  int render_frame_id_;

  // The source that provides video frames.
  std::unique_ptr<media::VideoCapturerSource> source_;

  enum State {
    STARTING,
    STARTED,
    STOPPING_FOR_RESTART,
    STOPPING_FOR_CHANGE_SOURCE,
    RESTARTING,
    STOPPED
  };
  State state_ = STOPPED;

  media::VideoCaptureParams capture_params_;
  blink::VideoCaptureDeliverFrameCB frame_callback_;
  DeviceCapturerFactoryCallback device_capturer_factory_callback_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamVideoCapturerSource);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_STREAM_MEDIA_STREAM_VIDEO_CAPTURER_SOURCE_H_
