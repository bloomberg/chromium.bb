// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

#include "base/stringprintf.h"

MockMediaStreamDispatcher::MockMediaStreamDispatcher()
    : MediaStreamDispatcher(NULL),
      request_id_(-1),
      stop_stream_counter_(0) {
}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    media_stream::StreamOptions components,
    const GURL& security_origin) {
  request_id_ = request_id;
  event_handler_ = event_handler;
  components_ = media_stream::StreamOptions(components.audio,
                                            components.video);
  security_origin_ = security_origin;

  stream_label_ = StringPrintf("%s%d","local_stream",request_id);
  audio_array_.clear();
  video_array_.clear();

  if (components.audio) {
    media_stream::StreamDeviceInfo audio;
    audio.device_id= "audio_device_id";
    audio.name ="audio microphone";
    audio.stream_type = content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE;
    audio.session_id = request_id;
    audio_array_.push_back(audio);
  }
  if (components.video) {
    media_stream::StreamDeviceInfo video;
    video.device_id= "video_device_id";
    video.name ="usb video camera";
    video.stream_type = content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE;
    video.session_id = request_id;
    video_array_.push_back(video);
  }
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
