// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_video_source.h"

namespace content {

MockMediaStreamVideoSource::MockMediaStreamVideoSource(
    MediaStreamDependencyFactory* factory,
    bool manual_get_supported_formats)
    : MediaStreamVideoSource(factory),
      manual_get_supported_formats_(manual_get_supported_formats),
      max_requested_height_(0),
      max_requested_width_(0),
      attempted_to_start_(false) {
  supported_formats_.push_back(
          media::VideoCaptureFormat(
              gfx::Size(MediaStreamVideoSource::kDefaultWidth,
                        MediaStreamVideoSource::kDefaultHeight),
              MediaStreamVideoSource::kDefaultFrameRate,
              media::PIXEL_FORMAT_I420));
}

MockMediaStreamVideoSource::~MockMediaStreamVideoSource() {}

void MockMediaStreamVideoSource::StartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(true);
}

void MockMediaStreamVideoSource::FailToStartMockedSource() {
  DCHECK(attempted_to_start_);
  attempted_to_start_ = false;
  OnStartDone(false);
}

void MockMediaStreamVideoSource::CompleteGetSupportedFormats() {
  OnSupportedFormats(supported_formats_);
}

void MockMediaStreamVideoSource::GetCurrentSupportedFormats(
    int max_requested_height,
    int max_requested_width) {
  max_requested_height_ = max_requested_height;
  max_requested_width_ = max_requested_width;

  if (!manual_get_supported_formats_)
    OnSupportedFormats(supported_formats_);
}

void MockMediaStreamVideoSource::StartSourceImpl(
    const media::VideoCaptureParams& params) {
  params_ = params;
  attempted_to_start_ = true;
}

void MockMediaStreamVideoSource::StopSourceImpl() {
}

}  // namespace content




