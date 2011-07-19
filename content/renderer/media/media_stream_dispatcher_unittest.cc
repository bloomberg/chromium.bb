// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/common/media/media_stream_messages.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kRouteId = 0;
const int kAudioSessionId = 3;
const int kVideoSessionId = 5;
const int kRequestId1 = 10;
const int kRequestId2 = 20;

class MockMediaStreamDispatcherEventHandler
    : public MediaStreamDispatcherEventHandler {
 public:
  MockMediaStreamDispatcherEventHandler()
      : request_id_(-1),
        audio_failed(false),
        video_failed(false) {}

  virtual void OnStreamGenerated(
      int request_id,
      const std::string &label,
      const media_stream::StreamDeviceInfoArray& audio_device_array,
      const media_stream::StreamDeviceInfoArray& video_device_array) {
    request_id_ = request_id;
    label_ = label;
  }

  virtual void OnStreamGenerationFailed(int request_id) {
    request_id_ = request_id;
  }

  virtual void OnAudioDeviceFailed(const std::string& label,
                                   int index) {
    audio_failed = true;
  }

  virtual void OnVideoDeviceFailed(const std::string& label,
                                   int index) {
    video_failed = true;
  }

  int request_id_;
  std::string label_;
  bool audio_failed;
  bool video_failed;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcherEventHandler);
};

}  // namespace

TEST(MediaStreamDispatcherTest, Basic) {
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(
      true, media_stream::StreamOptions::kFacingUser);
  std::string security_origin;

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get(), components,
                             security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->GenerateStream(kRequestId2, handler.get(), components,
                             security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  media_stream::StreamDeviceInfoArray audio_device_array(1);
  media_stream::StreamDeviceInfo audio_device_info;
  audio_device_info.name = "Microphone";
  audio_device_info.stream_type = media_stream::kAudioCapture;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.stream_type = media_stream::kVideoCapture;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the creation of stream1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
      kRouteId, ipc_request_id1, stream_label1,
      audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);

  // Complete the creation of stream2.
  std::string stream_label2 = std::string("stream2");
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
        kRouteId, ipc_request_id2, stream_label2,
        audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId2);
  EXPECT_EQ(handler->label_, stream_label2);

  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(2));

  // Check the session_id of stream2.
  EXPECT_EQ(dispatcher->audio_session_id(stream_label2, 0), kAudioSessionId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0), kVideoSessionId);

  // Stop stream2.
  dispatcher->StopStream(stream_label2);
  EXPECT_EQ(dispatcher->audio_session_id(stream_label2, 0),
            media_stream::StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0),
            media_stream::StreamDeviceInfo::kNoId);

  // Stop stream1.
  dispatcher->StopStream(stream_label1);
  EXPECT_EQ(dispatcher->audio_session_id(stream_label1, 0),
            media_stream::StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0),
            media_stream::StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
}

TEST(MediaStreamDispatcherTest, TestFailure) {
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(
      true, media_stream::StreamOptions::kFacingUser);
  std::string security_origin;

  // Test failure when creating a stream.
  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get(), components,
                             security_origin);
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerationFailed(
      kRouteId, ipc_request_id1));

  // Verify that the request have been completed.
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));

  // Create a new stream.
  ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get(), components,
                             security_origin);

  media_stream::StreamDeviceInfoArray audio_device_array(1);
  media_stream::StreamDeviceInfo audio_device_info;
  audio_device_info.name = "Microphone";
  audio_device_info.stream_type = media_stream::kAudioCapture;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.stream_type = media_stream::kVideoCapture;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the creation of stream1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
      kRouteId, ipc_request_id1, stream_label1,
      audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);

  // Test failure of the video device.
  dispatcher->OnMessageReceived(
      MediaStreamHostMsg_VideoDeviceFailed(kRouteId, stream_label1, 0));
  EXPECT_EQ(handler->video_failed, true);
  EXPECT_EQ(handler->audio_failed, false);
  // Make sure the audio device still exist but not the video device.
  EXPECT_EQ(dispatcher->audio_session_id(stream_label1, 0), kAudioSessionId);

  // Test failure of the audio device.
  dispatcher->OnMessageReceived(
      MediaStreamHostMsg_AudioDeviceFailed(kRouteId, stream_label1, 0));
  EXPECT_EQ(handler->audio_failed, true);

  // Stop stream1.
  dispatcher->StopStream(stream_label1);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
}
