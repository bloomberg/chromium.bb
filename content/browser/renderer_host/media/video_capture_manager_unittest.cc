// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureManager.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "media/video/capture/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

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

// Needed as an input argument to StartCaptureForClient().
class MockFrameObserver : public VideoCaptureControllerEventHandler {
 public:
  MOCK_METHOD1(OnError, void(const VideoCaptureControllerID& id));

  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE {}
  virtual void OnBufferDestroyed(const VideoCaptureControllerID& id,
                               int buffer_id) OVERRIDE {}
  virtual void OnBufferReady(
      const VideoCaptureControllerID& id,
      int buffer_id,
      base::Time timestamp,
      const media::VideoCaptureFormat& format) OVERRIDE {}
  virtual void OnEnded(const VideoCaptureControllerID& id) OVERRIDE {}

  void OnGotControllerCallback(VideoCaptureControllerID) {}
};

// Test class
class VideoCaptureManagerTest : public testing::Test {
 public:
  VideoCaptureManagerTest() : next_client_id_(1) {}
  virtual ~VideoCaptureManagerTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    listener_.reset(new MockMediaStreamProviderListener());
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    vcm_ = new VideoCaptureManager();
    vcm_->UseFakeDevice();
    vcm_->Register(listener_.get(), message_loop_->message_loop_proxy().get());
    frame_observer_.reset(new MockFrameObserver());
  }

  virtual void TearDown() OVERRIDE {}

  void OnGotControllerCallback(
      VideoCaptureControllerID id,
      base::Closure quit_closure,
      bool expect_success,
      const base::WeakPtr<VideoCaptureController>& controller) {
    if (expect_success) {
      ASSERT_TRUE(controller);
      ASSERT_TRUE(0 == controllers_.count(id));
      controllers_[id] = controller.get();
    } else {
      ASSERT_TRUE(NULL == controller);
    }
    quit_closure.Run();
  }

  VideoCaptureControllerID StartClient(int session_id, bool expect_success) {
    media::VideoCaptureParams params;
    params.session_id = session_id;
    params.requested_format = media::VideoCaptureFormat(
        320, 240, 30, media::ConstantResolutionVideoCaptureDevice);

    VideoCaptureControllerID client_id(next_client_id_++);
    base::RunLoop run_loop;
    vcm_->StartCaptureForClient(
        params, base::kNullProcessHandle, client_id, frame_observer_.get(),
        base::Bind(&VideoCaptureManagerTest::OnGotControllerCallback,
                   base::Unretained(this), client_id, run_loop.QuitClosure(),
                   expect_success));
    run_loop.Run();
    return client_id;
  }

  void StopClient(VideoCaptureControllerID client_id) {
    ASSERT_TRUE(1 == controllers_.count(client_id));
    vcm_->StopCaptureForClient(controllers_[client_id], client_id,
                               frame_observer_.get());
    controllers_.erase(client_id);
  }

  int next_client_id_;
  std::map<VideoCaptureControllerID, VideoCaptureController*> controllers_;
  scoped_refptr<VideoCaptureManager> vcm_;
  scoped_ptr<MockMediaStreamProviderListener> listener_;
  scoped_ptr<base::MessageLoop> message_loop_;
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
  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  StopClient(client_id);
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
  EXPECT_CALL(*listener_, Opened(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);
  EXPECT_CALL(*frame_observer_, OnError(_)).Times(1);
  EXPECT_CALL(*listener_, Closed(MEDIA_DEVICE_VIDEO_CAPTURE, _)).Times(1);

  vcm_->EnumerateDevices(MEDIA_DEVICE_VIDEO_CAPTURE);

  // Wait to get device callback.
  message_loop_->RunUntilIdle();

  MediaStreamType stream_type = MEDIA_DEVICE_VIDEO_CAPTURE;
  std::string device_name("device_doesnt_exist");
  std::string device_id("id_doesnt_exist");
  StreamDeviceInfo dummy_device(stream_type, device_name, device_id);

  // This should fail with an error to the controller.
  int session_id = vcm_->Open(dummy_device);
  VideoCaptureControllerID client_id = StartClient(session_id, true);
  message_loop_->RunUntilIdle();

  StopClient(client_id);
  vcm_->Close(session_id);
  message_loop_->RunUntilIdle();

  vcm_->Unregister();
}

// Start a device without calling Open, using a non-magic ID.
TEST_F(VideoCaptureManagerTest, StartInvalidSession) {
  StartClient(22, false);

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

  VideoCaptureControllerID client_id = StartClient(video_session_id, true);

  // Close will stop the running device, an assert will be triggered in
  // VideoCaptureManager destructor otherwise.
  vcm_->Close(video_session_id);
  StopClient(client_id);

  // Wait to check callbacks before removing the listener
  message_loop_->RunUntilIdle();
  vcm_->Unregister();
}

}  // namespace content
