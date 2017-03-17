// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_

#include <string>

#include "base/logging.h"
#include "content/common/content_export.h"
#include "media/capture/video_capture_types.h"
#include "third_party/webrtc/base/optional.h"

namespace blink {
class WebMediaConstraints;
}

namespace content {

class CONTENT_EXPORT VideoContentCaptureSourceSelectionResult {
 public:
  // Creates a result without value and with an empty failed constraint name.
  VideoContentCaptureSourceSelectionResult();

  // Creates a result without value and with the given |failed_constraint_name|.
  // Does not take ownership of |failed_constraint_name|, so it must be null or
  // point to a string that remains accessible.
  explicit VideoContentCaptureSourceSelectionResult(
      const char* failed_constraint_name);

  // Creates a result with the given values.
  VideoContentCaptureSourceSelectionResult(
      std::string device_id,
      const rtc::Optional<bool>& noise_reduction,
      media::VideoCaptureParams capture_params);

  VideoContentCaptureSourceSelectionResult(
      const VideoContentCaptureSourceSelectionResult& other);
  VideoContentCaptureSourceSelectionResult& operator=(
      const VideoContentCaptureSourceSelectionResult& other);
  VideoContentCaptureSourceSelectionResult(
      VideoContentCaptureSourceSelectionResult&& other);
  VideoContentCaptureSourceSelectionResult& operator=(
      VideoContentCaptureSourceSelectionResult&& other);
  ~VideoContentCaptureSourceSelectionResult();

  bool HasValue() const { return failed_constraint_name_ == nullptr; }

  // Accessors.
  const char* failed_constraint_name() const { return failed_constraint_name_; }
  const std::string& device_id() const {
    DCHECK(HasValue());
    return device_id_;
  }
  const rtc::Optional<bool>& noise_reduction() const {
    DCHECK(HasValue());
    return noise_reduction_;
  }
  media::VideoCaptureParams capture_params() const {
    DCHECK(HasValue());
    return capture_params_;
  }

  // Convenience accessors for fields embedded in the |capture_params_| field.
  int Height() const;
  int Width() const;
  float FrameRate() const;
  media::ResolutionChangePolicy ResolutionChangePolicy() const;

 private:
  const char* failed_constraint_name_;
  std::string device_id_;
  rtc::Optional<bool> noise_reduction_;
  media::VideoCaptureParams capture_params_;
};

// This function performs source and source-settings selection for content
// video capture based on the given |constraints|.
VideoContentCaptureSourceSelectionResult CONTENT_EXPORT
SelectVideoContentCaptureSourceSettings(
    const blink::WebMediaConstraints& constraints);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_CONSTRAINTS_UTIL_VIDEO_CONTENT_H_
