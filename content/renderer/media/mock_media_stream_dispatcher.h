// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_

#include <string>

#include "base/macros.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "url/origin.h"

namespace content {

// This class is a mock implementation of MediaStreamDispatcher.
class MockMediaStreamDispatcher : public MediaStreamDispatcher {
 public:
  MockMediaStreamDispatcher();
  ~MockMediaStreamDispatcher() override;

  void GenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
      const StreamControls& controls,
      const url::Origin& security_origin,
      bool is_procesing_user_gesture) override;
  void CancelGenerateStream(
      int request_id,
      const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler)
      override;
  void StopStreamDevice(const MediaStreamDevice& device) override;
  bool IsStream(const std::string& label) override;
  int video_session_id(const std::string& label, int index) override;
  int audio_session_id(const std::string& label, int index) override;

  int audio_input_request_id() const { return audio_input_request_id_; }
  int request_stream_counter() const { return request_stream_counter_; }
  void IncrementSessionId() { ++session_id_; }
  void TestSameId() { test_same_id_ = true; }

  int stop_audio_device_counter() const { return stop_audio_device_counter_; }
  int stop_video_device_counter() const { return stop_video_device_counter_; }

  const std::string& stream_label() const { return stream_label_;}
  const MediaStreamDevices& audio_devices() const { return audio_devices_; }
  const MediaStreamDevices& video_devices() const { return video_devices_; }

 private:
  void AddAudioDeviceToArray(bool matched_output, const std::string& device_id);
  void AddVideoDeviceToArray(bool facing_user, const std::string& device_id);

  int audio_input_request_id_;
  base::WeakPtr<MediaStreamDispatcherEventHandler> event_handler_;
  int request_stream_counter_;
  int stop_audio_device_counter_;
  int stop_video_device_counter_;

  std::string stream_label_;
  int session_id_;
  bool test_same_id_;
  MediaStreamDevices audio_devices_;
  MediaStreamDevices video_devices_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DISPATCHER_H_
