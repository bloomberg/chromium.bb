// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
#define CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "media/base/audio_parameters.h"
#include "media/base/video_facing.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace content {

// Types of media streams.
enum MediaStreamType {
  MEDIA_NO_SERVICE = 0,

  // A device provided by the operating system (e.g., webcam input).
  MEDIA_DEVICE_AUDIO_CAPTURE,
  MEDIA_DEVICE_VIDEO_CAPTURE,

  // Mirroring of a browser tab.
  MEDIA_TAB_AUDIO_CAPTURE,
  MEDIA_TAB_VIDEO_CAPTURE,

  // Desktop media sources.
  MEDIA_DESKTOP_VIDEO_CAPTURE,

  // Capture system audio (post-mix loopback stream).
  MEDIA_DESKTOP_AUDIO_CAPTURE,

  NUM_MEDIA_TYPES
};

// Types of media stream requests that can be made to the media controller.
enum MediaStreamRequestType {
  MEDIA_DEVICE_ACCESS = 0,
  MEDIA_GENERATE_STREAM,
  MEDIA_OPEN_DEVICE_PEPPER_ONLY  // Only used in requests made by Pepper.
};

// Elements in this enum should not be deleted or rearranged; the only
// permitted operation is to add new elements before NUM_MEDIA_REQUEST_RESULTS.
enum MediaStreamRequestResult {
  MEDIA_DEVICE_OK = 0,
  MEDIA_DEVICE_PERMISSION_DENIED = 1,
  MEDIA_DEVICE_PERMISSION_DISMISSED = 2,
  MEDIA_DEVICE_INVALID_STATE = 3,
  MEDIA_DEVICE_NO_HARDWARE = 4,
  MEDIA_DEVICE_INVALID_SECURITY_ORIGIN = 5,
  MEDIA_DEVICE_TAB_CAPTURE_FAILURE = 6,
  MEDIA_DEVICE_SCREEN_CAPTURE_FAILURE = 7,
  MEDIA_DEVICE_CAPTURE_FAILURE = 8,
  MEDIA_DEVICE_CONSTRAINT_NOT_SATISFIED = 9,
  MEDIA_DEVICE_TRACK_START_FAILURE_AUDIO = 10,
  MEDIA_DEVICE_TRACK_START_FAILURE_VIDEO = 11,
  MEDIA_DEVICE_NOT_SUPPORTED = 12,
  MEDIA_DEVICE_FAILED_DUE_TO_SHUTDOWN = 13,
  MEDIA_DEVICE_KILL_SWITCH_ON = 14,
  NUM_MEDIA_REQUEST_RESULTS
};

using CameraCalibration =
    media::VideoCaptureDeviceDescriptor::CameraCalibration;

// Convenience predicates to determine whether the given type represents some
// audio or some video device.
CONTENT_EXPORT bool IsAudioInputMediaType(MediaStreamType type);
CONTENT_EXPORT bool IsVideoMediaType(MediaStreamType type);
CONTENT_EXPORT bool IsScreenCaptureMediaType(MediaStreamType type);

// TODO(xians): Change the structs to classes.
// Represents one device in a request for media stream(s).
struct CONTENT_EXPORT MediaStreamDevice {
  static const int kNoId;

  MediaStreamDevice();

  MediaStreamDevice(MediaStreamType type,
                    const std::string& id,
                    const std::string& name);

  MediaStreamDevice(MediaStreamType type,
                    const std::string& id,
                    const std::string& name,
                    media::VideoFacingMode facing);

  MediaStreamDevice(MediaStreamType type,
                    const std::string& id,
                    const std::string& name,
                    int sample_rate,
                    int channel_layout,
                    int frames_per_buffer);

  MediaStreamDevice(const MediaStreamDevice& other);

  ~MediaStreamDevice();

  bool IsSameDevice(const MediaStreamDevice& other_device) const;

  // The device's type.
  MediaStreamType type;

  // The device's unique ID.
  std::string id;

  // The facing mode for video capture device.
  media::VideoFacingMode video_facing;

  // The device id of a matched output device if any (otherwise empty).
  // Only applicable to audio devices.
  base::Optional<std::string> matched_output_device_id;

  // The device's "friendly" name. Not guaranteed to be unique.
  std::string name;

  // Contains the device properties of the capture device. It's valid only when
  // the type of device is audio (i.e. IsAudioInputMediaType returns true).
  media::AudioParameters input =
      media::AudioParameters::UnavailableDeviceParams();

  // Id for this capture session. Unique for all sessions of the same type.
  int session_id = kNoId;

  // This field is optional and available only for some camera models.
  base::Optional<CameraCalibration> camera_calibration;
};

using MediaStreamDevices = std::vector<MediaStreamDevice>;

// Represents a request for media streams (audio/video).
// TODO(vrk,justinlin,wjia): Figure out a way to share this code cleanly between
// vanilla WebRTC, Tab Capture, and Pepper Video Capture. Right now there is
// Tab-only stuff and Pepper-only stuff being passed around to all clients,
// which is icky.
struct CONTENT_EXPORT MediaStreamRequest {
  MediaStreamRequest(int render_process_id,
                     int render_frame_id,
                     int page_request_id,
                     const GURL& security_origin,
                     bool user_gesture,
                     MediaStreamRequestType request_type,
                     const std::string& requested_audio_device_id,
                     const std::string& requested_video_device_id,
                     MediaStreamType audio_type,
                     MediaStreamType video_type,
                     bool disable_local_echo);

  MediaStreamRequest(const MediaStreamRequest& other);

  ~MediaStreamRequest();

  // This is the render process id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_process_id;

  // This is the render frame id for the renderer associated with generating
  // frames for a MediaStream. Any indicators associated with a capture will be
  // displayed for this renderer.
  int render_frame_id;

  // The unique id combined with render_process_id and render_frame_id for
  // identifying this request. This is used for cancelling request.
  int page_request_id;

  // The WebKit security origin for the current request (e.g. "html5rocks.com").
  GURL security_origin;

  // Set to true if the call was made in the context of a user gesture.
  bool user_gesture;

  // Stores the type of request that was made to the media controller. Right now
  // this is only used to distinguish between WebRTC and Pepper requests, as the
  // latter should not be subject to user approval but only to policy check.
  // Pepper requests are signified by the |MEDIA_OPEN_DEVICE| value.
  MediaStreamRequestType request_type;

  // Stores the requested raw device id for physical audio or video devices.
  std::string requested_audio_device_id;
  std::string requested_video_device_id;

  // Flag to indicate if the request contains audio.
  MediaStreamType audio_type;

  // Flag to indicate if the request contains video.
  MediaStreamType video_type;

  // Flag for desktop or tab share to indicate whether to prevent the captured
  // audio being played out locally.
  bool disable_local_echo;

  // True if all ancestors of the requesting frame have the same origin.
  bool all_ancestors_have_same_origin;
};

// Interface used by the content layer to notify chrome about changes in the
// state of a media stream. Instances of this class are passed to content layer
// when MediaStream access is approved using MediaResponseCallback.
class MediaStreamUI {
 public:
  virtual ~MediaStreamUI() {}

  // Called when MediaStream capturing is started. Chrome layer can call |stop|
  // to stop the stream. Returns the platform-dependent window ID for the UI, or
  // 0 if not applicable.
  virtual gfx::NativeViewId OnStarted(const base::Closure& stop) = 0;
};

// Callback used return results of media access requests.
using MediaResponseCallback =
    base::Callback<void(const MediaStreamDevices& devices,
                        MediaStreamRequestResult result,
                        std::unique_ptr<MediaStreamUI> ui)>;
}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_MEDIA_STREAM_REQUEST_H_
