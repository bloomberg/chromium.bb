// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureManager.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using testing::SaveArg;
using ::testing::Return;

namespace content {

// Listener class used to track progress of VideoCaptureManager test.
class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MockMediaStreamProviderListener() {}
  ~MockMediaStreamProviderListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, int));
  MOCK_METHOD2(Closed, void(MediaStreamType, int));
  MOCK_METHOD2(DevicesEnumerated, void(MediaStreamType,
                                       const StreamDeviceInfoArray&));
  MOCK_METHOD3(Error, void(MediaStreamType, int,
                           MediaStreamProviderError));
};  // class MockMediaStreamProviderListener

// Needed as an input argument to Start().
class MockFrameObserver : public media::VideoCaptureDevice::EventHandler {
 public:
  virtual void OnError() OVERRIDE {}
  virtual void OnFrameInfo(
      const media::VideoCaptureCapability& info) OVERRIDE {}
  virtual void OnIncomingCapturedFrame(const uint8* data,
                                       int length,
                                       base::Time timestamp,
                                       int rotation,
                                       bool flip_vert,
                                       bool flip_horiz) OVERRIDE {}
  virtual void OnIncomingCapturedVideoFrame(media::VideoFrame* frame,
                                            base::Time timestamp) OVERRIDE {}
};

// Test class
class VideoCaptureManagerTest : public testing::Test {
 public:
  VideoCaptureManagerTest() {}
  virtual ~VideoCaptureManagerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    listener_.reset(new MockMediaStreamProviderListener());
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    vcm_ = new VideoCaptureManager();
    vcm_->UseFakeDevice();
    vcm_->Register(listener_.get(), message_loop_->message_loop_proxy());
    frame_observer_.reset(new MockFrameObserver());
  }

  virtual void TearDown() OVERRIDE {}

  scoped_refptr<VideoCaptureManager> vcm_;
  scoped_ptr<MockMediaStreamProviderListener> listener_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<MockFrameObserver> frame_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManagerTest);
};

// Test cases

// Try to open, start, stop and close a device.
TEST_F(VideoCaptureManagerTest, CreateAndClose) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1).WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());

  media::VideoCaptureParams capture_params;
  capture_params.session_id = video_session_id;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;
  vcm_->Start(capture_params, frame_observer_.get());

  vcm_->Stop(video_session_id, base::Closure());
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open the same device twice.
TEST_F(VideoCaptureManagerTest, OpenTwice) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1).WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id_first = vcm_->Open(devices.front());

  // This should trigger an error callback with error code
  // 'kDeviceAlreadyInUse'.
  int video_session_id_second = vcm_->Open(devices.front());
  EXPECT_NE(video_session_id_first, video_session_id_second);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open two different devices.
TEST_F(VideoCaptureManagerTest, OpenTwo) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1).WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(2);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  StreamDeviceInfoArray::iterator it = devices.begin();

  int video_session_id_first = vcm_->Open(*it);
  ++it;
  int video_session_id_second = vcm_->Open(*it);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Try open a non-existing device.
TEST_F(VideoCaptureManagerTest, OpenNotExisting) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1).WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Error(MEDIA_DEVICE_VIDEO_CAPTURE,
                                _, kDeviceNotAvailable))
      .Times(1);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  MediaStreamType stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  StreamDeviceInfo dummy_device(stream_type, device_name, device_id, false);

  // This should fail with error code 'kDeviceNotAvailable'.
  vcm_->Open(dummy_device);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Start a device using "magic" id, i.e. call Start without calling Open.
TEST_F(VideoCaptureManagerTest, StartUsingId) {
  InSequence s;
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);

  media::VideoCaptureParams capture_params;
  capture_params.session_id = VideoCaptureManager::kStartOpenSessionId;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;

  // Start shall trigger the Open callback.
  vcm_->Start(capture_params, frame_observer_.get());

  // Stop shall trigger the Close callback
  vcm_->Stop(VideoCaptureManager::kStartOpenSessionId, base::Closure());

  // Wait to check callbacks before removing the listener.
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

// Open and start a device, close it before calling Stop.
TEST_F(VideoCaptureManagerTest, CloseWithoutStop) {
  StreamDeviceInfoArray devices;

  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1).WillOnce(SaveArg<1>(&devices));
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  int video_session_id = vcm_->Open(devices.front());

  media::VideoCaptureParams capture_params;
  capture_params.session_id = video_session_id;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;
  vcm_->Start(capture_params, frame_observer_.get());

  // Close will stop the running device, an assert will be triggered in
  // VideoCaptureManager destructor otherwise.
  vcm_->Close(video_session_id);
  vcm_->Stop(video_session_id, base::Closure());

  // Wait to check callbacks before removing the listener
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

}  // namespace content
