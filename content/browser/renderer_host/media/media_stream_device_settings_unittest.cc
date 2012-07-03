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

namespace media_stream {

// Test class.
class MediaStreamDeviceSettingsTest
    : public ::testing::Test,
      public SettingsRequester {
 public:
  MediaStreamDeviceSettingsTest() : label_("dummy_stream") {}

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

  void CreateDummyRequest() {
    int dummy_render_process_id = 1;
    int dummy_render_view_id = 1;
    StreamOptions components(true, false);
    GURL security_origin;
    device_settings_->RequestCaptureDeviceUsage(label_,
                                                dummy_render_process_id,
                                                dummy_render_view_id,
                                                components,
                                                security_origin);

    // Setup the dummy available device for the reqest.
    media_stream::StreamDeviceInfoArray audio_device_array(1);
    media_stream::StreamDeviceInfo dummy_audio_device;
    dummy_audio_device.name = "Microphone";
    dummy_audio_device.stream_type =
        content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE;
    dummy_audio_device.session_id = 1;
    audio_device_array[0] = dummy_audio_device;
    device_settings_->AvailableDevices(label_,
                                       dummy_audio_device.stream_type,
                                       audio_device_array);
  }

  scoped_ptr<MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<MediaStreamDeviceSettings> device_settings_;
  const std::string label_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamDeviceSettingsTest);
};

TEST_F(MediaStreamDeviceSettingsTest, GenerateRequest) {
  CreateDummyRequest();

  // Expecting an error callback triggered by the non-existing
  // RenderViewHostImpl.
  EXPECT_CALL(*this, SettingsError(label_))
  .Times(1);
}

TEST_F(MediaStreamDeviceSettingsTest, GenerateAndRemoveRequest) {
  CreateDummyRequest();

  // Remove the current request, it should not crash.
  device_settings_->RemovePendingCaptureRequest(label_);
}

}  // namespace media_stream
