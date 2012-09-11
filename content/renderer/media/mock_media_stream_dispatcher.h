// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_

#include <string>

#include "content/renderer/media/media_stream_dispatcher.h"
#include "googleurl/src/gurl.h"

// This class is a mock implementation of MediaStreamDispatcher.
class MockMediaStreamDispatcher : public MediaStreamDispatcher {
 public:
  MockMediaStreamDispatcher();
  virtual ~MockMediaStreamDispatcher();

  virtual void GenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>&,
      media_stream::StreamOptions components,
      const GURL&) OVERRIDE;
  virtual void GenerateStreamForDevice(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>&,
      media_stream::StreamOptions components,
      const std::string& device_id,
      const GURL&) OVERRIDE;
  virtual void StopStream(const std::string& label) OVERRIDE;
  virtual bool IsStream(const std::string& label) OVERRIDE;
  virtual int video_session_id(const std::string& label, int index) OVERRIDE;
  virtual int audio_session_id(const std::string& label, int index) OVERRIDE;

  int request_id() const { return request_id_; }
  int stop_stream_counter() const { return stop_stream_counter_; }
  const std::string& stream_label() const { return stream_label_;}
  media_stream::StreamDeviceInfoArray audio_array() const {
    return audio_array_;
  }
  media_stream::StreamDeviceInfoArray video_array() const {
    return video_array_;
  }

 private:
  int request_id_;
  base::WeakPtr<MediaStreamDispatcherEventHandler> event_handler_;
  int stop_stream_counter_;

  std::string stream_label_;
  media_stream::StreamDeviceInfoArray audio_array_;
  media_stream::StreamDeviceInfoArray video_array_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcher);
};

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
