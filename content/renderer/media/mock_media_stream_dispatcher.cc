// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mock_media_stream_dispatcher.h"

#include "base/strings/string_number_conversions.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"

// Used for ID for output devices and for matching output device ID for input
// devices.
const char kAudioOutputDeviceIdPrefix[] = "audio_output_device_id";

namespace content {

MockMediaStreamDispatcher::MockMediaStreamDispatcher()
    : MediaStreamDispatcher(nullptr),
      audio_input_request_id_(-1),
      request_stream_counter_(0),
      stop_audio_device_counter_(0),
      stop_video_device_counter_(0),
      session_id_(0),
      test_same_id_(false) {}

MockMediaStreamDispatcher::~MockMediaStreamDispatcher() {}

void MockMediaStreamDispatcher::GenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler,
    const StreamControls& controls,
    bool is_processing_user_gesture) {
  // Audio and video share the same request so we use |audio_input_request_id_|
  // only.
  audio_input_request_id_ = request_id;

  stream_label_ = "local_stream" + base::IntToString(request_id);
  audio_devices_.clear();
  video_devices_.clear();

  if (controls.audio.requested) {
    AddAudioDeviceToArray(false, controls.audio.device_id);
  }
  if (controls.video.requested) {
    AddVideoDeviceToArray(true, controls.video.device_id);
  }
  ++request_stream_counter_;
}

void MockMediaStreamDispatcher::CancelGenerateStream(
    int request_id,
    const base::WeakPtr<MediaStreamDispatcherEventHandler>& event_handler) {
  EXPECT_EQ(request_id, audio_input_request_id_);
}

void MockMediaStreamDispatcher::StopStreamDevice(
    const MediaStreamDevice& device) {
  if (IsAudioInputMediaType(device.type)) {
    ++stop_audio_device_counter_;
    return;
  }
  if (IsVideoMediaType(device.type)) {
    ++stop_video_device_counter_;
    return;
  }
  NOTREACHED();
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

void MockMediaStreamDispatcher::AddAudioDeviceToArray(
    bool matched_output,
    const std::string& device_id) {
  MediaStreamDevice audio_device;
  audio_device.id =
      (test_same_id_ ? "test_id" : device_id) + base::IntToString(session_id_);
  audio_device.name = "microphone";
  audio_device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio_device.video_facing = media::MEDIA_VIDEO_FACING_NONE;
  if (matched_output) {
    audio_device.matched_output_device_id =
        kAudioOutputDeviceIdPrefix + base::IntToString(session_id_);
  }
  audio_device.session_id = session_id_;
  audio_device.input = media::AudioParameters::UnavailableDeviceParams();
  audio_devices_.push_back(audio_device);
}

void MockMediaStreamDispatcher::AddVideoDeviceToArray(
    bool facing_user,
    const std::string& device_id) {
  MediaStreamDevice video_device;
  video_device.id =
      (test_same_id_ ? "test_id" : device_id) + base::IntToString(session_id_);
  video_device.name = "usb video camera";
  video_device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device.video_facing = facing_user
                                  ? media::MEDIA_VIDEO_FACING_USER
                                  : media::MEDIA_VIDEO_FACING_ENVIRONMENT;
  video_device.session_id = session_id_;
  video_devices_.push_back(video_device);
}

}  // namespace content
