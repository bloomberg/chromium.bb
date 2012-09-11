// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_device_settings.h"
#include "content/browser/renderer_host/media/media_stream_settings_requester.h"
#include "content/common/media/media_stream_options.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;
using testing::_;

namespace media_stream {

// Test class.
class MediaStreamDeviceSettingsTest
    : public ::testing::Test,
      public SettingsRequester {
 public:
  MediaStreamDeviceSettingsTest() {}

  // Mock implementation of SettingsRequester;
  MOCK_METHOD2(DevicesAccepted, void(const std::string&,
                                     const StreamDeviceInfoArray&));
  MOCK_METHOD1(SettingsError, void(const std::string&));

 protected:
  virtual void SetUp() {
    message_loop_.reset(new MessageLoop(MessageLoop::TYPE_IO));
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));
    device_settings_.reset(new MediaStreamDeviceSettings(this));
  }

  virtual void TearDown() {
    message_loop_->RunAllPending();
  }

  void CreateDummyRequest(const std::string& label, bool audio, bool video) {
    int dummy_render_process_id = 1;
    int dummy_render_view_id = 1;
    StreamOptions components(audio, video);
    GURL security_origin;
    device_settings_->RequestCaptureDeviceUsage(label,
                                                dummy_render_process_id,
                                                dummy_render_view_id,
                                                components,
                                                security_origin);
    if (audio)
      CreateAudioDeviceForRequset(label);

    if (video)
      CreateVideoDeviceForRequset(label);
  }

  void CreateAudioDeviceForRequset(const std::string& label) {
    // Setup the dummy available device for the request.
    media_stream::StreamDeviceInfoArray audio_device_array(1);
    media_stream::StreamDeviceInfo dummy_audio_device;
    dummy_audio_device.name = "Microphone";
    dummy_audio_device.stream_type = content::MEDIA_DEVICE_AUDIO_CAPTURE;
    dummy_audio_device.session_id = 1;
    audio_device_array[0] = dummy_audio_device;
    device_settings_->AvailableDevices(label,
                                       dummy_audio_device.stream_type,
                                       audio_device_array);
  }

  void CreateVideoDeviceForRequset(const std::string& label) {
    // Setup the dummy available device for the request.
    media_stream::StreamDeviceInfoArray video_device_array(1);
    media_stream::StreamDeviceInfo dummy_video_device;
    dummy_video_device.name = "camera";
    dummy_video_device.stream_type = content::MEDIA_DEVICE_VIDEO_CAPTURE;
    dummy_video_device.session_id = 1;
    video_device_array[0] = dummy_video_device;
    device_settings_->AvailableDevices(label,
                                       dummy_video_device.stream_type,
                                       video_device_array);
  }

  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<MediaStreamDeviceSettings> device_settings_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamDeviceSettingsTest);
};

TEST_F(MediaStreamDeviceSettingsTest, GenerateRequest) {
  const std::string label = "dummy_label";
  CreateDummyRequest(label, true, false);

  // Expecting an error callback triggered by the non-existing
  // RenderViewHostImpl.
  EXPECT_CALL(*this, SettingsError(label));
}

TEST_F(MediaStreamDeviceSettingsTest, GenerateAndRemoveRequest) {
  const std::string label = "label";
  CreateDummyRequest(label, true, false);

  // Remove the current request, it should not crash.
  device_settings_->RemovePendingCaptureRequest(label);
}

TEST_F(MediaStreamDeviceSettingsTest, HandleRequestUsingFakeUI) {
  device_settings_->UseFakeUI();

  const std::string label = "label";
  CreateDummyRequest(label, true, true);

  // Remove the current request, it should not crash.
  EXPECT_CALL(*this, DevicesAccepted(label, _));
}

TEST_F(MediaStreamDeviceSettingsTest, CreateMultipleRequestsAndCancelTheFirst) {
  device_settings_->UseFakeUI();

  // Create the first audio request.
  const std::string label_1 = "label_1";
  CreateDummyRequest(label_1, true, false);

  // Create the second video request.
  const std::string label_2 = "label_2";
  CreateDummyRequest(label_2, false, true);

  // Create the third audio and video request.
  const std::string label_3 = "label_3";
  CreateDummyRequest(label_3, true, true);

  // Remove the first request which has been brought to the UI.
  device_settings_->RemovePendingCaptureRequest(label_1);

  // We should get callbacks from the rest of the requests.
  EXPECT_CALL(*this, DevicesAccepted(label_2, _));
  EXPECT_CALL(*this, DevicesAccepted(label_3, _));
}

TEST_F(MediaStreamDeviceSettingsTest, CreateMultipleRequestsAndCancelTheLast) {
  device_settings_->UseFakeUI();

  // Create the first audio request.
  const std::string label_1 = "label_1";
  CreateDummyRequest(label_1, true, false);

  // Create the second video request.
  const std::string label_2 = "label_2";
  CreateDummyRequest(label_2, false, true);

  // Create the third audio and video request.
  const std::string label_3 = "label_3";
  CreateDummyRequest(label_3, true, true);

  // Remove the last request which is pending in the queue.
  device_settings_->RemovePendingCaptureRequest(label_3);

  // We should get callbacks from the rest of the requests.
  EXPECT_CALL(*this, DevicesAccepted(label_1, _));
  EXPECT_CALL(*this, DevicesAccepted(label_2, _));
}

}  // namespace media_stream
