// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_dispatcher.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_dispatcher_eventhandler.h"
#include "content/renderer/media/mock_mojo_media_stream_dispatcher_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

const int kAudioSessionId = 3;
const int kVideoSessionId = 5;
const int kScreenSessionId = 7;
const int kRenderId = 9;
const int kRequestId1 = 10;
const int kRequestId2 = 20;

class MockMediaStreamDispatcherEventHandler
    : public MediaStreamDispatcherEventHandler,
      public base::SupportsWeakPtr<MockMediaStreamDispatcherEventHandler> {
 public:
  MockMediaStreamDispatcherEventHandler() : request_id_(-1) {}

  void OnStreamGenerated(int request_id,
                         const std::string& label,
                         const MediaStreamDevices& audio_devices,
                         const MediaStreamDevices& video_devices) override {
    request_id_ = request_id;
    label_ = label;
    if (audio_devices.size()) {
      DCHECK_EQ(audio_devices.size(), 1U);
      audio_device_ = audio_devices[0];
    }
    if (video_devices.size()) {
      DCHECK_EQ(video_devices.size(), 1U);
      video_device_ = video_devices[0];
    }
  }

  void OnStreamGenerationFailed(int request_id,
                                MediaStreamRequestResult result) override {
    request_id_ = request_id;
  }

  void OnDeviceStopped(const std::string& label,
                       const MediaStreamDevice& device) override {
    device_stopped_label_ = label;
    if (IsVideoMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(video_device_));
    if (IsAudioInputMediaType(device.type))
      EXPECT_TRUE(device.IsSameDevice(audio_device_));
  }

  void ResetStoredParameters() {
    request_id_ = -1;
    label_.clear();
    device_stopped_label_.clear();
    audio_device_ = MediaStreamDevice();
    video_device_ = MediaStreamDevice();
  }

  int request_id_;
  std::string label_;
  std::string device_stopped_label_;
  MediaStreamDevice audio_device_;
  MediaStreamDevice video_device_;
};

class MediaStreamDispatcherTest : public ::testing::Test {
 public:
  MediaStreamDispatcherTest()
      : dispatcher_(std::make_unique<MediaStreamDispatcher>(nullptr)),
        handler_(std::make_unique<MockMediaStreamDispatcherEventHandler>()),
        controls_(true, true),
        security_origin_(url::Origin::Create(GURL("http://test.com"))) {
    mojom::MediaStreamDispatcherHostPtr dispatcher_host =
        mock_dispatcher_host_.CreateInterfacePtrAndBind();
    dispatcher_->dispatcher_host_ = std::move(dispatcher_host);
  }

  // Generates a request for a MediaStream and returns the request id that is
  // used in mojo IPC. Use this returned id in CompleteGenerateStream to
  // identify the request.
  int GenerateStream(int request_id) {
    int next_ipc_id = dispatcher_->next_ipc_id_;
    dispatcher_->GenerateStream(request_id, handler_->AsWeakPtr(), controls_,
                                true);
    return next_ipc_id;
  }

  // CompleteGenerateStream calls the MediaStreamDispathcer::OnStreamGenerated.
  // |ipc_id| must be the the id returned by GenerateStream.
  std::string CompleteGenerateStream(int ipc_id,
                                     int request_id) {
    MediaStreamDevices audio_devices(controls_.audio.requested ? 1 : 0);
    if (controls_.audio.requested) {
      MediaStreamDevice audio_device;
      audio_device.name = "Microphone";
      audio_device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
      audio_device.session_id = kAudioSessionId;
      audio_devices[0] = audio_device;
    }

    MediaStreamDevices video_devices(controls_.video.requested ? 1 : 0);
    if (controls_.video.requested) {
      MediaStreamDevice video_device;
      video_device.name = "Camera";
      video_device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
      video_device.session_id = kVideoSessionId;
      video_devices[0] = video_device;
    }

    std::string label = "stream" + base::IntToString(ipc_id);

    handler_->ResetStoredParameters();
    dispatcher_->OnStreamGenerated(ipc_id, label, audio_devices, video_devices);

    EXPECT_EQ(handler_->request_id_, request_id);
    EXPECT_EQ(handler_->label_, label);

    if (controls_.audio.requested)
      EXPECT_EQ(dispatcher_->audio_session_id(label), kAudioSessionId);

    if (controls_.video.requested)
      EXPECT_EQ(dispatcher_->video_session_id(label), kVideoSessionId);

    return label;
  }

  void OnDeviceOpened(base::Closure quit_closure,
                      const MediaStreamDevice& input_device,
                      const std::string& input_label,
                      bool success,
                      const std::string& label,
                      const MediaStreamDevice& device) {
    dispatcher_->AddStream(input_label, input_device);
    quit_closure.Run();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  MockMojoMediaStreamDispatcherHost mock_dispatcher_host_;
  std::unique_ptr<MediaStreamDispatcher> dispatcher_;
  std::unique_ptr<MockMediaStreamDispatcherEventHandler> handler_;
  StreamControls controls_;
  url::Origin security_origin_;
};

TEST_F(MediaStreamDispatcherTest, GenerateStreamAndStopDevices) {
  int ipc_request_id1 = GenerateStream(kRequestId1);
  int ipc_request_id2 = GenerateStream(kRequestId2);
  EXPECT_NE(ipc_request_id1, ipc_request_id2);

  // Complete the creation of stream1.
  const std::string& label1 =
      CompleteGenerateStream(ipc_request_id1, kRequestId1);

  // Complete the creation of stream2.
  const std::string& label2 =
      CompleteGenerateStream(ipc_request_id2, kRequestId2);

  // Stop the actual audio device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->audio_device_);
  EXPECT_EQ(dispatcher_->audio_session_id(label1), MediaStreamDevice::kNoId);
  EXPECT_EQ(dispatcher_->audio_session_id(label2), MediaStreamDevice::kNoId);

  // Stop the actual video device and verify that there is no valid
  // |session_id|.
  dispatcher_->StopStreamDevice(handler_->video_device_);
  EXPECT_EQ(dispatcher_->video_session_id(label1), MediaStreamDevice::kNoId);
  EXPECT_EQ(dispatcher_->video_session_id(label2), MediaStreamDevice::kNoId);
}

TEST_F(MediaStreamDispatcherTest, TestFailure) {
  // Test failure when creating a stream.
  int ipc_request_id1 = GenerateStream(kRequestId1);
  dispatcher_->OnStreamGenerationFailed(ipc_request_id1,
                                        MEDIA_DEVICE_PERMISSION_DENIED);

  // Verify that the request have been completed.
  EXPECT_EQ(handler_->request_id_, kRequestId1);
  EXPECT_EQ(dispatcher_->requests_.size(), size_t(0));

  // Create a new stream.
  ipc_request_id1 = GenerateStream(kRequestId1);

  MediaStreamDevices audio_devices(1);
  MediaStreamDevice audio_device;
  audio_device.name = "Microphone";
  audio_device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio_device.session_id = kAudioSessionId;
  audio_devices[0] = audio_device;

  MediaStreamDevices video_devices(1);
  MediaStreamDevice video_device;
  video_device.name = "Camera";
  video_device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device.session_id = kVideoSessionId;
  video_devices[0] = video_device;

  // Complete the creation of stream1.
  std::string stream_label1 = std::string("stream1");
  dispatcher_->OnStreamGenerated(ipc_request_id1, stream_label1, audio_devices,
                                 video_devices);
  EXPECT_EQ(handler_->request_id_, kRequestId1);
  EXPECT_EQ(handler_->label_, stream_label1);
  EXPECT_EQ(dispatcher_->video_session_id(stream_label1), kVideoSessionId);
}

TEST_F(MediaStreamDispatcherTest, CancelGenerateStream) {
  int ipc_request_id1 = GenerateStream(kRequestId1);
  GenerateStream(kRequestId2);

  EXPECT_EQ(2u, dispatcher_->requests_.size());
  dispatcher_->CancelGenerateStream(kRequestId2, handler_->AsWeakPtr());
  EXPECT_EQ(1u, dispatcher_->requests_.size());

  // Complete the creation of stream1.
  MediaStreamDevice audio_device;
  audio_device.name = "Microphone";
  audio_device.type = MEDIA_DEVICE_AUDIO_CAPTURE;
  audio_device.session_id = kAudioSessionId;
  MediaStreamDevices audio_devices(1);
  audio_devices[0] = audio_device;

  MediaStreamDevice video_device;
  video_device.name = "Camera";
  video_device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device.session_id = kVideoSessionId;
  MediaStreamDevices video_devices(1);
  video_devices[0] = video_device;

  std::string stream_label1 = "stream1";
  dispatcher_->OnStreamGenerated(ipc_request_id1, stream_label1, audio_devices,
                                 video_devices);
  EXPECT_EQ(handler_->request_id_, kRequestId1);
  EXPECT_EQ(handler_->label_, stream_label1);
  EXPECT_EQ(0u, dispatcher_->requests_.size());
}

// Test that the MediaStreamDispatcherEventHandler is notified when the message
// OnDeviceStopped is received.
TEST_F(MediaStreamDispatcherTest, DeviceClosed) {
  int ipc_request_id = GenerateStream(kRequestId1);
  const std::string& label =
      CompleteGenerateStream(ipc_request_id, kRequestId1);

  dispatcher_->OnDeviceStopped(label, handler_->video_device_);
  // Verify that MediaStreamDispatcherEventHandler::OnDeviceStopped has been
  // called.
  EXPECT_EQ(label, handler_->device_stopped_label_);
  EXPECT_EQ(dispatcher_->video_session_id(label), MediaStreamDevice::kNoId);
}

TEST_F(MediaStreamDispatcherTest, GetNonScreenCaptureDevices) {
  MediaStreamDevice video_device;
  video_device.name = "Camera";
  video_device.id = "device_path";
  video_device.type = MEDIA_DEVICE_VIDEO_CAPTURE;
  video_device.session_id = kVideoSessionId;

  MediaStreamDevice screen_device;
  screen_device.name = "Screen";
  screen_device.id = "screen_capture";
  screen_device.type = MEDIA_DESKTOP_VIDEO_CAPTURE;
  screen_device.session_id = kScreenSessionId;

  std::string stream_label1 = std::string("stream1");
  std::string stream_label2 = std::string("stream2");

  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 0u);

  // OpenDevice request 1
  base::RunLoop run_loop1;
  mock_dispatcher_host_.OpenDevice(
      kRenderId, kRequestId1, video_device.id, MEDIA_DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDispatcherTest::OnDeviceOpened,
                     base::Unretained(this), run_loop1.QuitClosure(),
                     video_device, stream_label1));
  run_loop1.Run();

  // OpenDevice request 2
  base::RunLoop run_loop2;
  mock_dispatcher_host_.OpenDevice(
      kRenderId, kRequestId2, screen_device.id, MEDIA_DEVICE_VIDEO_CAPTURE,
      base::BindOnce(&MediaStreamDispatcherTest::OnDeviceOpened,
                     base::Unretained(this), run_loop2.QuitClosure(),
                     screen_device, stream_label2));
  run_loop2.Run();

  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 2u);

  // Only the device with type MEDIA_DEVICE_VIDEO_CAPTURE will be returned.
  MediaStreamDevices video_devices = dispatcher_->GetNonScreenCaptureDevices();
  EXPECT_EQ(video_devices.size(), 1u);
  EXPECT_EQ(video_devices[0].type, MEDIA_DEVICE_VIDEO_CAPTURE);

  // Close the device from request 2.
  dispatcher_->RemoveStream(stream_label2);
  EXPECT_EQ(dispatcher_->video_session_id(stream_label2),
            MediaStreamDevice::kNoId);

  // Close the device from request 1.
  dispatcher_->RemoveStream(stream_label1);
  EXPECT_EQ(dispatcher_->video_session_id(stream_label1),
            MediaStreamDevice::kNoId);

  // Verify that the request have been completed.
  EXPECT_EQ(dispatcher_->label_stream_map_.size(), 0u);
}

}  // namespace content
