// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_APPLY_CONSTRAINTS_PROCESSOR_H_
#define CONTENT_RENDERER_MEDIA_APPLY_CONSTRAINTS_PROCESSOR_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "content/common/media/media_devices.mojom.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "media/capture/video_capture_types.h"
#include "third_party/WebKit/public/web/WebApplyConstraintsRequest.h"

namespace blink {
class WebString;
}

namespace content {

class MediaStreamVideoSource;
class MediaStreamVideoTrack;

// ApplyConstraintsProcessor is responsible for processing applyConstraints()
// requests. Only one applyConstraints() request can be processed at a time.
// ApplyConstraintsProcessor must be created, called and destroyed on the main
// render thread. There should be only one ApplyConstraintsProcessor per frame.
class CONTENT_EXPORT ApplyConstraintsProcessor {
 public:
  using MediaDevicesDispatcherCallback =
      base::RepeatingCallback<const ::mojom::MediaDevicesDispatcherHostPtr&()>;
  ApplyConstraintsProcessor(
      MediaDevicesDispatcherCallback media_devices_dispatcher_cb);
  ~ApplyConstraintsProcessor();

  // Starts processing of |request|. When processing of |request| is complete,
  // it notifies by invoking |callback|.
  // This method must be called only if there is no request currently being
  // processed.
  void ProcessRequest(const blink::WebApplyConstraintsRequest& request,
                      base::OnceClosure callback);

 private:
  // Helpers for video device-capture requests.
  void ProcessVideoDeviceRequest();
  void MaybeStopSourceForRestart(const media::VideoCaptureFormats& formats);
  void MaybeSourceStoppedForRestart(
      MediaStreamVideoSource::RestartResult result);
  void FindNewFormatAndRestart(const media::VideoCaptureFormats& formats);
  void MaybeSourceRestarted(MediaStreamVideoSource::RestartResult result);
  void FinalizeVideoDeviceRequest();
  VideoCaptureSettings SelectVideoDeviceSettings(
      media::VideoCaptureFormats formats);

  // Helpers for video requests.
  void ProcessVideoRequest();
  MediaStreamVideoTrack* GetCurrentVideoTrack();
  MediaStreamVideoSource* GetCurrentVideoSource();
  bool AbortIfVideoRequestStateInvalid();  // Returns true if aborted.

  // General helpers
  void ApplyConstraintsSucceeded();
  void ApplyConstraintsFailed(const char* failed_constraint_name);
  void CannotApplyConstraints(const blink::WebString& message);
  void CleanupRequest(base::OnceClosure web_request_callback);
  const ::mojom::MediaDevicesDispatcherHostPtr& GetMediaDevicesDispatcher();

  // ApplyConstraints requests are processed sequentially. |current_request_|
  // contains the request currently being processed, if any.
  // |video_source_| and |request_completed_cb_| are the video source and
  // reply callback for the current request.
  blink::WebApplyConstraintsRequest current_request_;
  MediaStreamVideoSource* video_source_ = nullptr;
  base::OnceClosure request_completed_cb_;

  MediaDevicesDispatcherCallback media_devices_dispatcher_cb_;
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ApplyConstraintsProcessor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplyConstraintsProcessor);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_APPLY_CONSTRAINTS_PROCESSOR_H_
