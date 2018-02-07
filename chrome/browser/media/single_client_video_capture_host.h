// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_SINGLE_CLIENT_VIDEO_CAPTURE_HOST_H_
#define CHROME_BROWSER_MEDIA_SINGLE_CLIENT_VIDEO_CAPTURE_HOST_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "content/public/common/media_stream_request.h"
#include "media/capture/mojo/video_capture.mojom.h"
#include "media/capture/video/video_frame_receiver.h"

namespace media {

// Implements a subset of mojom::VideoCaptureHost to proxy between a
// content::LaunchedVideoCaptureDevice and a single client. On Start(), uses the
// provided DeviceLauncherCreateCallback to launch a video capture device that
// outputs frames to |this|. The frames are received through the
// VideoFrameReceiver interface and are forwarded to |observer| through the
// mojom::VideoCaptureObserver interface.
// Instances of this class must be operated from the same thread that is reqired
// by the DeviceLauncherCreateCallback.
class SingleClientVideoCaptureHost final : public mojom::VideoCaptureHost,
                                           public VideoFrameReceiver {
 public:
  using DeviceLauncherCreateCallback = base::RepeatingCallback<
      std::unique_ptr<content::VideoCaptureDeviceLauncher>()>;
  SingleClientVideoCaptureHost(const std::string& device_id,
                               content::MediaStreamType type,
                               const VideoCaptureParams& params,
                               DeviceLauncherCreateCallback callback);
  ~SingleClientVideoCaptureHost() override;

  // mojom::VideoCaptureHost implementations
  // |device_id| and |session_id| are ignored since there will be only one
  // device and one client. |params| is also ignored since it is already set
  // through the constructor.
  void Start(int32_t device_id,
             int32_t session_id,
             const media::VideoCaptureParams& params,
             mojom::VideoCaptureObserverPtr observer) override;
  void Stop(int32_t device_id) override;
  void Pause(int32_t device_id) override;
  void Resume(int32_t device_id,
              int32_t session_id,
              const media::VideoCaptureParams& params) override;
  void RequestRefreshFrame(int32_t device_id) override;
  void ReleaseBuffer(int32_t device_id,
                     int32_t buffer_id,
                     double consumer_resource_utilization) override;
  void GetDeviceSupportedFormats(
      int32_t device_id,
      int32_t session_id,
      GetDeviceSupportedFormatsCallback callback) override;
  void GetDeviceFormatsInUse(int32_t device_id,
                             int32_t session_id,
                             GetDeviceFormatsInUseCallback callback) override;

  // media::VideoFrameReceiver implementations
  using Buffer = VideoCaptureDevice::Client::Buffer;
  void OnNewBufferHandle(
      int buffer_id,
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
          handle_provider) override;
  void OnFrameReadyInBuffer(
      int buffer_id,
      int frame_feedback_id,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          buffer_read_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;
  void OnStartedUsingGpuDecode() override;

  void OnDeviceLaunched(
      std::unique_ptr<content::LaunchedVideoCaptureDevice> device);
  void OnDeviceLaunchFailed();
  void OnDeviceLaunchAborted();

 private:
  // Reports the |consumer_resource_utilization| and removes the buffer context.
  void OnFinishedConsumingBuffer(int buffer_context_id,
                                 double consumer_resource_utilization);

  const std::string device_id_;
  const content::MediaStreamType type_;
  const VideoCaptureParams params_;
  const DeviceLauncherCreateCallback device_launcher_callback_;

  mojom::VideoCaptureObserverPtr observer_;
  std::unique_ptr<content::LaunchedVideoCaptureDevice> launched_device_;

  // Unique ID assigned for the next buffer provided by OnNewBufferHandle().
  int next_buffer_context_id_ = 0;

  // Records the assigned buffer_context_id for buffers that are not retired.
  // The |buffer_id| provided by OnNewBufferHandle() is used as the key.
  base::flat_map<int, int> id_map_;

  // Tracks the the retired buffers that are still held by |observer_|.
  base::flat_set<int> retired_buffers_;

  // Records the |frame_feedback_id| and |buffer_read_permission| provided by
  // OnFrameReadyInBuffer(). The key is the assigned buffer context id. Each
  // entry is removed after the |observer_| finishes consuming the buffer by
  // calling ReleaseBuffer(). When Stop() is called, all the buffers are cleared
  // immediately.
  using BufferContext = std::pair<
      int,
      std::unique_ptr<
          VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>>;
  base::flat_map<int, BufferContext> buffer_context_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SingleClientVideoCaptureHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientVideoCaptureHost);
};

}  // namespace media

#endif  // CHROME_BROWSER_MEDIA_SINGLE_CLIENT_VIDEO_CAPTURE_HOST_H_
