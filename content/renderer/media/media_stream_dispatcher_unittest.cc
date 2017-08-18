// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/common/media/media_stream.mojom.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

const int kAudioSessionId = 3;
const int kVideoSessionId = 5;
const int kScreenSessionId = 7;
const int kRequestId1 = 10;
const int kRequestId2 = 20;

class MockMojoMediaStreamDispatcherHost
    : public mojom::MediaStreamDispatcherHost {
 public:
  MockMojoMediaStreamDispatcherHost() {}

  MOCK_METHOD5(
      GenerateStream,
      void(int32_t, int32_t, const StreamControls&, const url::Origin&, bool));
  MOCK_METHOD2(CancelGenerateStream, void(int32_t, int32_t));
  MOCK_METHOD2(StopStreamDevice, void(int32_t, const std::string&));
  MOCK_METHOD5(OpenDevice,
               void(int32_t,
                    int32_t,
                    const std::string&,
                    MediaStreamType,
                    const url::Origin&));
  MOCK_METHOD1(CloseDevice, void(const std::string&));
  MOCK_METHOD3(SetCapturingLinkSecured, void(int32_t, MediaStreamType, bool));
  MOCK_METHOD1(StreamStarted, void(const std::string&));
};

class MockMediaStreamDispatcherEventHandler
    : public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MockMediaStreamDispatcherEventHandler> {
 public:
  MockMediaStreamDispatcherEventHandler() : request_id_(-1) {}

  void OnStreamGenerated(
      int request_id,
      const std::string& label,
      const StreamDeviceInfoArray& audio_device_array,
      const StreamDeviceInfoArray& video_device_array) override {
    request_id_ = request_id;
    label_ = label;
    if (audio_device_array.size()) {
      DCHECK(audio_device_array.size() == 1);
      audio_device_ = audio_device_array[0];
    }
    if (video_device_array.size()) {
      DCHECK(video_device_array.size() == 1);
      video_device_ = video_device_array[0];
    }
  }

  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) override {
    request_id_ = request_id;
  }

  void OnDeviceStopped(const std::string& label,
                       const StreamDeviceInfo& device_info) override {
    device_stopped_label_ = label;
    if (IsVideoMediaType(device_info.device.type)) {
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(video_device_, device_info));
    }
    if (IsAudioInputMediaType(device_info.device.type)) {
      EXPECT_TRUE(StreamDeviceInfo::IsEqual(audio_device_, device_info));
    }
  }

  void OnDeviceOpened(int request_id,
                      const std::string& label,
                      const StreamDeviceInfo& video_device) override {
    request_id_ = request_id;
    label_ = label;
  }

  void OnDeviceOpenFailed(int request_id) override { request_id_ = request_id; }

  void ResetStoredParameters() {
    request_id_ = -1;
    label_ = "";
    device_stopped_label_ = "";
    audio_device_ = StreamDeviceInfo();
    video_device_ = StreamDeviceInfo();
  }

  int request_id_;
  std::string label_;
  std::string device_stopped_label_;
  StreamDeviceInfo audio_device_;
  StreamDeviceInfo video_device_;
};

class MediaStreamDispatcherUnderTest : public MediaStreamDispatcher {
 public:
  MediaStreamDispatcherUnderTest() : MediaStreamDispatcher(nullptr) {}

  using MediaStreamDispatcher::GetNextIpcIdForTest;
  using RenderFrameObserver::OnMessageReceived;
};

class MediaStreamDispatcherTest : public ::testing::Test {
 public:
  MediaStreamDispatcherTest()
      : dispatcher_(new MediaStreamDispatcherUnderTest()),
        handler_(new MockMediaStreamDispatcherEventHandler),
        security_origin_(GURL("http://test.com")) {
    dispatcher_->dispatcher_host_ = &mock_dispatcher_host_;
  }

  // Generates a request for a MediaStream and returns the request id that is
  // used in IPC. Use this returned id in CompleteGenerateStream to identify
  // the request.
  int GenerateStream(const StreamControls& controls, int request_id) {
    int next_ipc_id = dispatcher_->GetNextIpcIdForTest();
    dispatcher_->GenerateStream(request_id, handler_.get()->AsWeakPtr(),
                                controls, security_origin_, true);
    return next_ipc_id;
  }

  // CompleteGenerateStream create a MediaStreamMsg_StreamGenerated instance
  // and call the MediaStreamDispathcer::OnMessageReceived. |ipc_id| must be the
  // the id returned by GenerateStream.
  std::string CompleteGenerateStream(int ipc_id,
                                     const StreamControls& controls,
                                     int request_id) {
    StreamDeviceInfoArray audio_device_array(controls.audio.requested ? 1 : 0);
    if (controls.audio.requested) {
      StreamDeviceInfo audio_device_info;
      audio_device_info.device.name = "Microphone";
      audio_device_info.device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
      audio_device_info.session_id = kAudioSessionId;
      audio_device_array[0] = audio_device_info;
    }

    StreamDeviceInfoArray video_device_array(controls.video.requested ? 1 : 0);
    if (controls.video.requested) {
      StreamDeviceInfo video_device_info;
      video_device_info.device.name = "Camera";
      video_device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
      video_device_info.session_id = kVideoSessionId;
      video_device_array[0] = video_device_info;
    }

    std::string label = "stream" + base::IntToString(ipc_id);

    handler_->ResetStoredParameters();
    dispatcher_->OnStreamGenerated(ipc_id, label, audio_device_array,
                                   video_device_array);

    EXPECT_EQ(handler_->request_id_, request_id);
    EXPECT_EQ(handler_->label_, label);

    if (controls.audio.requested)
      EXPECT_EQ(dispatcher_->audio_session_id(label, 0), kAudioSessionId);

    if (controls.video.requested)
      EXPECT_EQ(dispatcher_->video_session_id(label, 0), kVideoSessionId);

    return label;
  }

 protected:
  base::MessageLoop message_loop_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  std::unique_ptr<MediaStreamDispatcherUnderTest> dispatcher_;
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler_;
  url::Origin security_origin_;
};

TEST_F(MediaStreamDispatcherTest, GenerateStreamAndStopDevices) {
  StreamControls controls(true, true);

  int ipc_request_id1 = GenerateStream(controls, kRequestId1);
  int ipc_request_id2 = GenerateStream(controls, kRequestId2);
  EXPECT_NE(ipc_request_id1, ipc_request_id2);

  // Complete the creation of stream1.
  const std::string& label1 =
      CompleteGenerateStream(ipc_request_id1, controls, kRequestId1);

  // Complete the creation of stream2.
  const std::string& label2 =
      CompleteGenerateStream(ipc_request_id2, controls, kRequestId2);

  // Stop the actual audio device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->audio_device_);
  EXPECT_EQ(dispatcher_->audio_session_id(label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher_->audio_session_id(label2, 0),
            StreamDeviceInfo::kNoId);

  // Stop the actual video device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->video_device_);
  EXPECT_EQ(dispatcher_->video_session_id(label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher_->video_session_id(label2, 0),
            StreamDeviceInfo::kNoId);
}

TEST_F(MediaStreamDispatcherTest, BasicVideoDevice) {
  auto dispatcher = base::MakeUnique<MediaStreamDispatcher>(nullptr);
  auto handler1 = base::MakeUnique<MockMediaStreamDispatcherEventHandler>();
  auto handler2 = base::MakeUnique<MockMediaStreamDispatcherEventHandler>();
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host;
  dispatcher->dispatcher_host_ = &mock_dispatcher_host;
  url::Origin security_origin;

  StreamDeviceInfoArray video_device_array(1);
  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.id = "device_path";
  video_device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->OpenDevice(kRequestId1, handler1.get()->AsWeakPtr(),
                         video_device_info.device.id,
                         MEDIA_DEVICE_VIDEO_CAPTURE, security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->OpenDevice(kRequestId2, handler1.get()->AsWeakPtr(),
                         video_device_info.device.id,
                         MEDIA_DEVICE_VIDEO_CAPTURE, security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(2));

  // Complete the OpenDevice of request 1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnDeviceOpened(ipc_request_id1, stream_label1, video_device_info);
  EXPECT_EQ(handler1->request_id_, kRequestId1);

  // Complete the OpenDevice of request 2.
  std::string stream_label2 = std::string("stream2");
  dispatcher->OnDeviceOpened(ipc_request_id2, stream_label2, video_device_info);
  EXPECT_EQ(handler1->request_id_, kRequestId2);

  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(2));

  // Check the video_session_id.
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0), kVideoSessionId);

  // Close the device from request 2.
  dispatcher->CloseDevice(stream_label2);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0),
            StreamDeviceInfo::kNoId);

  // Close the device from request 1.
  dispatcher->CloseDevice(stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0),
            StreamDeviceInfo::kNoId);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), size_t(0));
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));
}

TEST_F(MediaStreamDispatcherTest, TestFailure) {
  auto dispatcher = base::MakeUnique<MediaStreamDispatcher>(nullptr);
  auto handler = base::MakeUnique<MockMediaStreamDispatcherEventHandler>();
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host;
  dispatcher->dispatcher_host_ = &mock_dispatcher_host;
  StreamControls components(true, true);
  url::Origin security_origin;

  // Test failure when creating a stream.
  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin, true);
  dispatcher->OnStreamGenerationFailed(ipc_request_id1,
                                       MEDIA_DEVICE_PERMISSION_DENIED);

  // Verify that the request have been completed.
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(dispatcher->requests_.size(), size_t(0));

  // Create a new stream.
  ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, security_origin, true);

  StreamDeviceInfoArray audio_device_array(1);
  StreamDeviceInfo audio_device_info;
  audio_device_info.device.name = "Microphone";
  audio_device_info.device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio_device_info.session_id = kAudioSessionId;
  audio_device_array[0] = audio_device_info;

  StreamDeviceInfoArray video_device_array(1);
  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device_info.session_id = kVideoSessionId;
  video_device_array[0] = video_device_info;

  // Complete the creation of stream1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnStreamGenerated(ipc_request_id1, stream_label1,
                                audio_device_array, video_device_array);
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0), kVideoSessionId);
}

TEST_F(MediaStreamDispatcherTest, CancelGenerateStream) {
  auto dispatcher = base::MakeUnique<MediaStreamDispatcher>(nullptr);
  auto handler = base::MakeUnique<MockMediaStreamDispatcherEventHandler>();
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host;
  dispatcher->dispatcher_host_ = &mock_dispatcher_host;
  StreamControls components(true, true);
  int ipc_request_id1 = dispatcher->next_ipc_id_;

  dispatcher->GenerateStream(kRequestId1, handler.get()->AsWeakPtr(),
                             components, url::Origin(), true);
  dispatcher->GenerateStream(kRequestId2, handler.get()->AsWeakPtr(),
                             components, url::Origin(), true);

  EXPECT_EQ(2u, dispatcher->requests_.size());
  dispatcher->CancelGenerateStream(kRequestId2, handler.get()->AsWeakPtr());
  EXPECT_EQ(1u, dispatcher->requests_.size());

  // Complete the creation of stream1.
  StreamDeviceInfo audio_device_info;
  audio_device_info.device.name = "Microphone";
  audio_device_info.device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio_device_info.session_id = kAudioSessionId;
  StreamDeviceInfoArray audio_device_array(1);
  audio_device_array[0] = audio_device_info;

  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device_info.session_id = kVideoSessionId;
  StreamDeviceInfoArray video_device_array(1);
  video_device_array[0] = video_device_info;

  std::string stream_label1 = "stream1";
  dispatcher->OnStreamGenerated(ipc_request_id1, stream_label1,
                                audio_device_array, video_device_array);
  EXPECT_EQ(handler->request_id_, kRequestId1);
  EXPECT_EQ(handler->label_, stream_label1);
  EXPECT_EQ(0u, dispatcher->requests_.size());
}

// Test that the MediaStreamDispatcherEventHandler is notified when the message
// OnDeviceStopped is received.
TEST_F(MediaStreamDispatcherTest, DeviceClosed) {
  StreamControls controls(true, true);

  int ipc_request_id = GenerateStream(controls, kRequestId1);
  const std::string& label =
      CompleteGenerateStream(ipc_request_id, controls, kRequestId1);

  dispatcher_->OnDeviceStopped(label, handler_->video_device_);
  // Verify that MediaStreamDispatcherEventHandler::OnDeviceStopped has been
  // called.
  EXPECT_EQ(label, handler_->device_stopped_label_);
  EXPECT_EQ(dispatcher_->video_session_id(label, 0),
            StreamDeviceInfo::kNoId);
}

TEST_F(MediaStreamDispatcherTest, GetNonScreenCaptureDevices) {
  auto dispatcher = base::MakeUnique<MediaStreamDispatcher>(nullptr);
  auto handler = base::MakeUnique<MockMediaStreamDispatcherEventHandler>();
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host;
  dispatcher->dispatcher_host_ = &mock_dispatcher_host;
  url::Origin security_origin;

  StreamDeviceInfo video_device_info;
  video_device_info.device.name = "Camera";
  video_device_info.device.id = "device_path";
  video_device_info.device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device_info.session_id = kVideoSessionId;

  StreamDeviceInfo screen_device_info;
  screen_device_info.device.name = "Screen";
  screen_device_info.device.id = "screen_capture";
  screen_device_info.device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  screen_device_info.session_id = kScreenSessionId;

  EXPECT_EQ(dispatcher->requests_.size(), 0u);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), 0u);

  int ipc_request_id1 = dispatcher->next_ipc_id_;
  dispatcher->OpenDevice(kRequestId1, handler.get()->AsWeakPtr(),
                         video_device_info.device.id,
                         MEDIA_DEVICE_VIDEO_CAPTURE, security_origin);
  int ipc_request_id2 = dispatcher->next_ipc_id_;
  EXPECT_NE(ipc_request_id1, ipc_request_id2);
  dispatcher->OpenDevice(kRequestId2, handler.get()->AsWeakPtr(),
                         screen_device_info.device.id,
                         MEDIA_DESKTOP_VIDEO_CAPTURE, security_origin);
  EXPECT_EQ(dispatcher->requests_.size(), 2u);

  // Complete the OpenDevice of request 1.
  std::string stream_label1 = std::string("stream1");
  dispatcher->OnDeviceOpened(ipc_request_id1, stream_label1, video_device_info);
  EXPECT_EQ(handler->request_id_, kRequestId1);

  // Complete the OpenDevice of request 2.
  std::string stream_label2 = std::string("stream2");
  dispatcher->OnDeviceOpened(ipc_request_id2, stream_label2,
                             screen_device_info);
  EXPECT_EQ(handler->request_id_, kRequestId2);

  EXPECT_EQ(dispatcher->requests_.size(), 0u);
  EXPECT_EQ(dispatcher->label_stream_map_.size(), 2u);

  // Only the device with type MEDIA_DEVICE_VIDEO_CAPTURE will be returned.
  StreamDeviceInfoArray video_device_array =
      dispatcher->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_device_array.size(), 1u);

  // Close the device from request 2.
  dispatcher->CloseDevice(stream_label2);
  EXPECT_EQ(dispatcher->video_session_id(stream_label2, 0),
            StreamDeviceInfo::kNoId);

  // Close the device from request 1.
  dispatcher->CloseDevice(stream_label1);
  EXPECT_EQ(dispatcher->video_session_id(stream_label1, 0),
            StreamDeviceInfo::kNoId);

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher->label_stream_map_.size(), 0u);
  EXPECT_EQ(dispatcher->requests_.size(), 0u);
}

}  // namespace content
