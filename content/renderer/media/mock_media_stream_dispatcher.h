// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_

#include <string>

#include "content/renderer/media/media_stream_dispatcher.h"

// This class is a mock implementation of MediaStreamDispatcher.
class MockMediaStreamDispatcher : public MediaStreamDispatcher {
 public:
  MockMediaStreamDispatcher();
  virtual ~MockMediaStreamDispatcher();

  virtual void GenerateStream(int request_id,
                              MediaStreamDispatcherEventHandler* event_handler,
                              media_stream::StreamOptions components,
                              const std::string& security_origin) OVERRIDE;
  virtual void StopStream(const std::string& label) OVERRIDE;
  virtual bool IsStream(const std::string& label) OVERRIDE;
  virtual int video_session_id(const std::string& label, int index) OVERRIDE;
  virtual int audio_session_id(const std::string& label, int index) OVERRIDE;

  int request_id() const { return request_id_; }
  MediaStreamDispatcherEventHandler* event_handler() const {
    return event_handler_;
  }
  media_stream::StreamOptions* components() const { return components_; }
  const std::string& security_origin() const { return security_origin_; }
  int stop_stream_counter() const { return stop_stream_counter_; }

 private:
  int request_id_;
  MediaStreamDispatcherEventHandler* event_handler_;
  media_stream::StreamOptions* components_;
  std::string security_origin_;
  int stop_stream_counter_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcher);
};

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
