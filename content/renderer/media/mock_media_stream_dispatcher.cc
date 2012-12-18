// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

#include "base/stringprintf.h"
#include "content/public/common/media_stream_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockMediaStreamDispatcher::MockMediaStreamDispatcher()
    : MediaStreamDispatcher(NULL),
      request_id_(-1),
      request_stream_counter_(0),
      stop_stream_counter_(0) {
}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamOptions& components,
    const GURL& url) {
  request_id_ = request_id;

  stream_label_ = StringPrintf("%s%d","local_stream",request_id);
  audio_array_.clear();
  video_array_.clear();

  if (IsAudioMediaType(components.audio_type)) {
    StreamDeviceInfo audio;
    audio.device.id = "audio_device_id";
    audio.device.name = "microphone";
    audio.device.type = components.audio_type;
    audio.session_id = request_id;
    audio_array_.push_back(audio);
  }
  if (IsVideoMediaType(components.video_type)) {
    StreamDeviceInfo video;
    video.device.id = "video_device_id";
    video.device.name = "usb video camera";
    video.device.type = components.video_type;
    video.session_id = request_id;
    video_array_.push_back(video);
  }
  ++request_stream_counter_;
}

void MockMediaStreamDispatcher::CancelGenerateStream(int request_id) {
  EXPECT_EQ(request_id, request_id_);
}

void MockMediaStreamDispatcher::StopStream(const std::string& label) {
  ++stop_stream_counter_;
}

bool MockMediaStreamDispatcher::IsStream(const std::string& label) {
  return true;
}

int MockMediaStreamDispatcher::video_session_id(const std::string& label,
                                                int index) {
  return -1;
}

int MockMediaStreamDispatcher::audio_session_id(const std::string& label,
                                                int index) {
  return -1;
}

}  // namespace content
