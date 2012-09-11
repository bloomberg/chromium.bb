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
using ::testing::Return;
using content::BrowserThread;

using content::BrowserThreadImpl;

namespace media_stream {

// Listener class used to track progress of VideoCaptureManager test.
class MockMediaStreamProviderListener : public MediaStreamProviderListener {
 public:
  MockMediaStreamProviderListener()
      : devices_() {
  }
  ~MockMediaStreamProviderListener() {}

  MOCK_METHOD2(Opened, void(MediaStreamType, int));
  MOCK_METHOD2(Closed, void(MediaStreamType, int));
  MOCK_METHOD1(DevicesEnumerated, void(const StreamDeviceInfoArray&));
  MOCK_METHOD3(Error, void(MediaStreamType, int,
                           MediaStreamProviderError));

  virtual void DevicesEnumerated(
      MediaStreamType stream_type,
      const StreamDeviceInfoArray& devices) OVERRIDE {
    devices_.clear();
    for (StreamDeviceInfoArray::const_iterator it = devices.begin();
        it != devices.end();
        ++it) {
      devices_.push_back(*it);
    }
    DevicesEnumerated(devices);
  }

  media_stream::StreamDeviceInfoArray devices_;
};  // class MockMediaStreamProviderListener

}  // namespace media_stream

namespace {

// Needed as an input argument to Start().
class MockFrameObserver : public media::VideoCaptureDevice::EventHandler {
 public:
  virtual void OnError() OVERRIDE {}
  void OnFrameInfo(const media::VideoCaptureCapability& info) {}
  virtual void OnIncomingCapturedFrame(const uint8* data, int length,
                                       base::Time timestamp) OVERRIDE {}
};

// Test class
class VideoCaptureManagerTest : public testing::Test {
 public:
  VideoCaptureManagerTest() {}
  virtual ~VideoCaptureManagerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    listener_.reset(new media_stream::MockMediaStreamProviderListener());
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    vcm_ = new media_stream::VideoCaptureManager();
    vcm_->UseFakeDevice();
    vcm_->Register(listener_.get(), message_loop_->message_loop_proxy());
    frame_observer_.reset(new MockFrameObserver());
  }

  virtual void TearDown() OVERRIDE {}

  scoped_refptr<media_stream::VideoCaptureManager> vcm_;
  scoped_ptr<media_stream::MockMediaStreamProviderListener> listener_;
  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<MockFrameObserver> frame_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureManagerTest);
};

// Test cases

// Try to open, start, stop and close a device.
TEST_F(VideoCaptureManagerTest, CreateAndClose) {
  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(_))
      .Times(1);
  EXPECT_CALL(*listener_,
              Opened(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);
  EXPECT_CALL(*listener_,
              Closed(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);

  vcm_->EnumerateDevices();

  // Wait to get device callback.
  message_loop_->RunAllPending();

  int video_session_id = vcm_->Open(listener_->devices_.front());

  media::VideoCaptureParams capture_params;
  capture_params.session_id = video_session_id;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;
  vcm_->Start(capture_params, frame_observer_.get());

  vcm_->Stop(video_session_id, base::Closure());
  vcm_->Close(video_session_id);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

// Open the same device twice.
TEST_F(VideoCaptureManagerTest, OpenTwice) {
  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(_))
      .Times(1);
  EXPECT_CALL(*listener_,
              Opened(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(2);
  EXPECT_CALL(*listener_,
              Closed(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(2);

  vcm_->EnumerateDevices();

  // Wait to get device callback.
  message_loop_->RunAllPending();

  int video_session_id_first = vcm_->Open(listener_->devices_.front());

  // This should trigger an error callback with error code
  // 'kDeviceAlreadyInUse'.
  int video_session_id_second = vcm_->Open(listener_->devices_.front());
  EXPECT_NE(video_session_id_first, video_session_id_second);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

// Open two different devices.
TEST_F(VideoCaptureManagerTest, OpenTwo) {
  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(_))
      .Times(1);
  EXPECT_CALL(*listener_,
              Opened(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(2);
  EXPECT_CALL(*listener_,
              Closed(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(2);

  vcm_->EnumerateDevices();

  // Wait to get device callback.
  message_loop_->RunAllPending();

  media_stream::StreamDeviceInfoArray::iterator it =
      listener_->devices_.begin();

  int video_session_id_first = vcm_->Open(*it);
  ++it;
  int video_session_id_second = vcm_->Open(*it);

  vcm_->Close(video_session_id_first);
  vcm_->Close(video_session_id_second);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

// Try open a non-existing device.
TEST_F(VideoCaptureManagerTest, OpenNotExisting) {
  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(_))
      .Times(1);
  EXPECT_CALL(*listener_, Error(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                _, media_stream::kDeviceNotAvailable))
      .Times(1);

  vcm_->EnumerateDevices();

  // Wait to get device callback.
  message_loop_->RunAllPending();

  media_stream::MediaStreamType stream_type =
      content::MEDIA_DEVICE_VIDEO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  media_stream::StreamDeviceInfo dummy_device(stream_type, device_name,
                                              device_id, false);

  // This should fail with error code 'kDeviceNotAvailable'.
  vcm_->Open(dummy_device);

  // Wait to check callbacks before removing the listener.
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

// Start a device using "magic" id, i.e. call Start without calling Open.
TEST_F(VideoCaptureManagerTest, StartUsingId) {
  InSequence s;
  EXPECT_CALL(*listener_,
              Opened(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);
  EXPECT_CALL(*listener_,
              Closed(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);

  media::VideoCaptureParams capture_params;
  capture_params.session_id =
      media_stream::VideoCaptureManager::kStartOpenSessionId;
  capture_params.width = 320;
  capture_params.height = 240;
  capture_params.frame_per_second = 30;

  // Start shall trigger the Open callback.
  vcm_->Start(capture_params, frame_observer_.get());

  // Stop shall trigger the Close callback
  vcm_->Stop(media_stream::VideoCaptureManager::kStartOpenSessionId,
             base::Closure());

  // Wait to check callbacks before removing the listener.
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

// Open and start a device, close it before calling Stop.
TEST_F(VideoCaptureManagerTest, CloseWithoutStop) {
  InSequence s;
  EXPECT_CALL(*listener_, DevicesEnumerated(_))
      .Times(1);
  EXPECT_CALL(*listener_,
              Opened(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);
  EXPECT_CALL(*listener_,
              Closed(content::MEDIA_DEVICE_VIDEO_CAPTURE, _))
      .Times(1);

  vcm_->EnumerateDevices();

  // Wait to get device callback.
  message_loop_->RunAllPending();

  int video_session_id = vcm_->Open(listener_->devices_.front());

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
  message_loop_->RunAllPending();
  vcm_->Unregister();
}

}  // namespace
