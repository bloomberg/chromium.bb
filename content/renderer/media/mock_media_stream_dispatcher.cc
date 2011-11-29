// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

MockMediaStreamDispatcher::MockMediaStreamDispatcher()
    : MediaStreamDispatcher(NULL),
      request_id_(-1),
      event_handler_(NULL),
      components_(NULL),
      stop_stream_counter_(0) {
}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    MediaStreamDispatcherEventHandler* event_handler,
    media_stream::StreamOptions components,
    const std::string& security_origin) {
  request_id_ = request_id;
  event_handler_ = event_handler;
  delete components_;
  components_ = new media_stream::StreamOptions(components.audio,
                                                components.video_option);
  security_origin_ = security_origin;
}

void MockMediaStreamDispatcher::StopStream(const std::string& label) {
  ++stop_stream_counter_;
}

bool MockMediaStreamDispatcher::IsStream(const std::string& label) {
  NOTIMPLEMENTED();
  return false;
}

int MockMediaStreamDispatcher::video_session_id(const std::string& label,
                                                int index) {
  NOTIMPLEMENTED();
  return -1;
}

int MockMediaStreamDispatcher::audio_session_id(const std::string& label,
                                                int index) {
  NOTIMPLEMENTED();
  return -1;
}
