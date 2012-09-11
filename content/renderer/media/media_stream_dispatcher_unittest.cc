// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "content/common/media/media_stream_messages.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_dispatcher.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kRouteId = 0;
const int kAudioSessionId = 3;
const int kVideoSessionId = 5;
const int kRequestId1 = 10;
const int kRequestId2 = 20;
const int kRequestId3 = 30;
const int kRequestId4 = 40;
static const char kLabel[] = "test";

const content::MediaStreamDeviceType kAudioType =
    content::MEDIA_DEVICE_AUDIO_CAPTURE;
const content::MediaStreamDeviceType kVideoType =
    content::MEDIA_DEVICE_VIDEO_CAPTURE;
const content::MediaStreamDeviceType kNoAudioType =
    content::MEDIA_NO_SERVICE;

class MockMediaStreamDispatcherEventHandler
    : public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MockMediaStreamDispatcherEventHandler> {
 public:
  MockMediaStreamDispatcherEventHandler()
      : request_id_(-1),
        audio_failed(false),
        video_failed(false) {}

  virtual void OnStreamGenerated(
      int request_id,
      const std::string &label,
      const media_stream::StreamDeviceInfoArray& audio_device_array,
      const media_stream::StreamDeviceInfoArray& video_device_array) OVERRIDE {
    request_id_ = request_id;
    label_ = label;
  }

  virtual void OnStreamGenerationFailed(int request_id) OVERRIDE {
    request_id_ = request_id;
  }

  virtual void OnAudioDeviceFailed(const std::string& label,
                                   int index) OVERRIDE {
    audio_failed = true;
  }

  virtual void OnVideoDeviceFailed(const std::string& label,
                                   int index) OVERRIDE {
    video_failed = true;
  }

  virtual void OnDevicesEnumerated(
      int request_id,
      const media_stream::StreamDeviceInfoArray& device_array) OVERRIDE {
    request_id_ = request_id;
  }

  virtual void OnDevicesEnumerationFailed(int request_id) OVERRIDE {
    request_id_ = request_id;
  }

  virtual void OnDeviceOpened(
      int request_id,
      const std::string& label,
      const media_stream::StreamDeviceInfo& video_device) OVERRIDE {
    request_id_ = request_id;
    label_ = label;
  }

  virtual void OnDeviceOpenFailed(int request_id) OVERRIDE {
    request_id_ = request_id;
  }

  int request_id_;
  std::string label_;
  bool audio_failed;
  bool video_failed;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDispatcherEventHandler);
};

}  // namespace

TEST(MediaStreamDispatcherTest, BasicStream) {
  scoped_ptr<MessageLoop> message_loop(new MessageLoop());
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(true, true);
  GURL security_origin;

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->GenerateStream(kRequestId2, handler.get()->AsWeakPtr(),
                             components, security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  media_stream::StreamDeviceInfoArray audio_device_array(1);
  media_stream::StreamDeviceInfo audio_device_info;
  audio_device_info.name = "Microphone";
  audio_device_info.stream_type = kAudioType;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.stream_type = kVideoType;
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

  // Verify that the request has been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
}

TEST(MediaStreamDispatcherTest, BasicStreamForDevice) {
  static const char kDeviceId[] = "/dev/video0";

  scoped_ptr<MessageLoop> message_loop(new MessageLoop());
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(kNoAudioType, kVideoType);
  GURL security_origin;

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStreamForDevice(kRequestId1, handler.get()->AsWeakPtr(),
                                      components, kDeviceId, security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->GenerateStreamForDevice(kRequestId2, handler.get()->AsWeakPtr(),
                                      components, kDeviceId, security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  // No audio requested.
  media_stream::StreamDeviceInfoArray audio_device_array;

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Fake Video Capture Device";
  video_device_info.stream_type = kVideoType;
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

  // Verify that the request has been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
}

TEST(MediaStreamDispatcherTest, BasicVideoDevice) {
  scoped_ptr<MessageLoop> message_loop(new MessageLoop());
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler1(new MockMediaStreamDispatcherEventHandler);
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler2(new MockMediaStreamDispatcherEventHandler);
  GURL security_origin;

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->EnumerateDevices(
      kRequestId1, handler1.get()->AsWeakPtr(),
      kVideoType,
      security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->EnumerateDevices(
      kRequestId2, handler2.get()->AsWeakPtr(),
      kVideoType,
      security_origin);
  EXPECT_EQ(dispatcher->video_enumeration_state_.requests.size(), size_t(2));

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.device_id = "device_path";
  video_device_info.stream_type = kVideoType;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the enumeration request and all requesters should receive reply.
  dispatcher->OnMessageReceived(MediaStreamMsg_DevicesEnumerated(
      kRouteId, ipc_request_id1, kLabel, video_device_array));
  EXPECT_EQ(handler1->request_id_, kRequestId1);
  EXPECT_EQ(handler2->request_id_, kRequestId2);

  EXPECT_EQ(dispatcher->video_enumeration_state_.requests.size(), size_t(2));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  int ipc_request_id3 = dispatcher->next_ipc_id_;
  dispatcher->OpenDevice(kRequestId3, handler1.get()->AsWeakPtr(),
                         video_device_info.device_id,
                         kVideoType,
                         security_origin);
  int ipc_request_id4 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id3, ipc_request_id4);
  dispatcher->OpenDevice(kRequestId4, handler1.get()->AsWeakPtr(),
                         video_device_info.device_id,
                         kVideoType,
                         security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  // Complete the OpenDevice of request 1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnMessageReceived(MediaStreamMsg_DeviceOpened(
      kRouteId, ipc_request_id3, stream_label1, video_device_info));
  EXPECT_EQ(handler1->request_id_, kRequestId3);

  // Complete the OpenDevice of request 2.
  std::string stream_label2 = std::string("stream2");
  dispatcher->OnMessageReceived(MediaStreamMsg_DeviceOpened(
      kRouteId, ipc_request_id4, stream_label2, video_device_info));
  EXPECT_EQ(handler1->request_id_, kRequestId4);

  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(2));

  // Check the video_session_id.
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0), kVideoSessionId);

  // Stop stream2.
  dispatcher->StopStream(stream_label2);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0),
            media_stream::StreamDeviceInfo::kNoId);

  // Stop stream1.
  dispatcher->StopStream(stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0),
            media_stream::StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
}

TEST(MediaStreamDispatcherTest, TestFailure) {
  scoped_ptr<MessageLoop> message_loop(new MessageLoop());
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(true, true);
  GURL security_origin;

  // Test failure when creating a stream.
  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin);
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerationFailed(
      kRouteId, ipc_request_id1));

  // Verify that the request have been completed.
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));

  // Create a new stream.
  ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin);

  media_stream::StreamDeviceInfoArray audio_device_array(1);
  media_stream::StreamDeviceInfo audio_device_info;
  audio_device_info.name = "Microphone";
  audio_device_info.stream_type = kAudioType;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  media_stream::StreamDeviceInfoArray video_device_array(1);
  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.stream_type = kVideoType;
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

TEST(MediaStreamDispatcherTest, CancelGenerateStream) {
  scoped_ptr<MessageLoop> message_loop(new MessageLoop());
  scoped_ptr<MediaStreamDispatcher> dispatcher(new MediaStreamDispatcher(NULL));
  scoped_ptr<MockMediaStreamDispatcherEventHandler>
      handler(new MockMediaStreamDispatcherEventHandler);
  media_stream::StreamOptions components(true, true);
  int ipc_request_id1 = dispatcher->next_ipc_id_;

  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, GURL());
  dispatcher->GenerateStream(kRequestId2, handler.get()->AsWeakPtr(),
                             components, GURL());

  EXPECT_EQ(2u, dispatcher->requests_.size());
  dispatcher->CancelGenerateStream(kRequestId2);
  EXPECT_EQ(1u, dispatcher->requests_.size());

  // Complete the creation of stream1.
  media_stream::StreamDeviceInfo audio_device_info;
  audio_device_info.name = "Microphone";
  audio_device_info.stream_type = kAudioType;
  audio_device_info.session_id = kAudioSessionId;
  media_stream::StreamDeviceInfoArray audio_device_array(1);
  audio_device_array[0] = audio_device_info;

  media_stream::StreamDeviceInfo video_device_info;
  video_device_info.name = "Camera";
  video_device_info.stream_type = kVideoType;
  video_device_info.session_id = kVideoSessionId;
  media_stream::StreamDeviceInfoArray video_device_array(1);
  video_device_array[0] = video_device_info;

  std::string stream_label1 = "stream1";
  dispatcher->OnMessageReceived(MediaStreamMsg_StreamGenerated(
      kRouteId, ipc_request_id1, stream_label1,
      audio_device_array, video_device_array));
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(0u, dispatcher->requests_.size());
}
