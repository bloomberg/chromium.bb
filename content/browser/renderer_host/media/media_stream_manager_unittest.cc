// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/common/media/media_stream_options.h"
#include "media/audio/audio_manager_base.h"
#if defined(OS_ANDROID)
#include "media/audio/android/audio_manager_android.h"
#elif defined(OS_LINUX) || defined(OS_OPENBSD)
#include "media/audio/linux/audio_manager_linux.h"
#elif defined(OS_MACOSX)
#include "media/audio/mac/audio_manager_mac.h"
#elif defined(OS_WIN)
#include "media/audio/win/audio_manager_win.h"
#endif
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace content {

#if defined(OS_LINUX) || defined(OS_OPENBSD)
typedef media::AudioManagerLinux AudioManagerPlatform;
#elif defined(OS_MACOSX)
typedef media::AudioManagerMac AudioManagerPlatform;
#elif defined(OS_WIN)
typedef media::AudioManagerWin AudioManagerPlatform;
#elif defined(OS_ANDROID)
typedef media::AudioManagerAndroid AudioManagerPlatform;
#endif


// This class mocks the audio manager and overrides the
// GetAudioInputDeviceNames() method to ensure that we can run our tests on
// the buildbots. media::AudioManagerBase
class MockAudioManager : public AudioManagerPlatform {
 public:
  MockAudioManager() {}
  virtual ~MockAudioManager() {}

  virtual void GetAudioInputDeviceNames(
      media::AudioDeviceNames* device_names) OVERRIDE {
    if (HasAudioInputDevices()) {
      AudioManagerBase::GetAudioInputDeviceNames(device_names);
    } else {
      device_names->push_back(media::AudioDeviceName("fake_device_name",
                                                     "fake_device_id"));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAudioManager);
};

class MediaStreamManagerTest : public ::testing::Test {
 public:
  MediaStreamManagerTest() {}

  MOCK_METHOD1(Response, void(const std::string&));
  void ResponseCallback(const std::string& label,
                        const MediaStreamDevices& devices) {
    Response(label);
    message_loop_->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }

  void WaitForResult() { message_loop_->Run(); }

 protected:
  virtual void SetUp() {
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_IO));
    ui_thread_.reset(new BrowserThreadImpl(BrowserThread::UI,
                                           message_loop_.get()));
    io_thread_.reset(new BrowserThreadImpl(BrowserThread::IO,
                                           message_loop_.get()));

    // Create our own MediaStreamManager.
    audio_manager_.reset(new MockAudioManager());
    media_stream_manager_.reset(new MediaStreamManager(audio_manager_.get()));

    // Use fake devices in order to run on the bots.
    media_stream_manager_->UseFakeDevice();
  }

  virtual void TearDown() {
    message_loop_->RunUntilIdle();

    // Delete the IO message loop to clean up the MediaStreamManager.
    message_loop_.reset();
  }

  std::string MakeMediaAccessRequest() {
    const int render_process_id = 1;
    const int render_view_id = 1;
    StreamOptions components(MEDIA_DEVICE_AUDIO_CAPTURE,
                             MEDIA_DEVICE_VIDEO_CAPTURE);
    const GURL security_origin;
    MediaRequestResponseCallback callback =
        base::Bind(&MediaStreamManagerTest::ResponseCallback,
                   base::Unretained(this));
    return media_stream_manager_->MakeMediaAccessRequest(render_process_id,
                                                         render_view_id,
                                                         components,
                                                         security_origin,
                                                         callback);
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<BrowserThreadImpl> ui_thread_;
  scoped_ptr<BrowserThreadImpl> io_thread_;
  scoped_ptr<media::AudioManager> audio_manager_;
  scoped_ptr<MediaStreamManager> media_stream_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamManagerTest);
};

TEST_F(MediaStreamManagerTest, MakeMediaAccessRequest) {
  std::string label = MakeMediaAccessRequest();

  // Expecting the callback will be triggered and quit the test.
  EXPECT_CALL(*this, Response(label));
  WaitForResult();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMediaAccessRequest) {
  std::string label = MakeMediaAccessRequest();
  // No callback is expected.
  media_stream_manager_->CancelRequest(label);
}

TEST_F(MediaStreamManagerTest, MakeMultipleRequests) {
  // First request.
  std::string label1 =  MakeMediaAccessRequest();

  // Second request.
  int render_process_id = 2;
  int render_view_id = 2;
  StreamOptions components(MEDIA_DEVICE_AUDIO_CAPTURE,
                           MEDIA_DEVICE_VIDEO_CAPTURE);
  GURL security_origin;
  MediaRequestResponseCallback callback =
      base::Bind(&MediaStreamManagerTest::ResponseCallback,
                 base::Unretained(this));
  std::string label2 = media_stream_manager_->MakeMediaAccessRequest(
      render_process_id,
      render_view_id,
      components,
      security_origin,
      callback);

  // Expecting the callbackS from requests will be triggered and quit the test.
  // Note, the callbacks might come in a different order depending on the
  // value of labels.
  EXPECT_CALL(*this, Response(_)).Times(2);
  WaitForResult();
}

TEST_F(MediaStreamManagerTest, MakeAndCancelMultipleRequests) {
  std::string label1 = MakeMediaAccessRequest();
  std::string label2 = MakeMediaAccessRequest();
  media_stream_manager_->CancelRequest(label1);

  // Expecting the callback from the second request will be triggered and
  // quit the test.
  EXPECT_CALL(*this, Response(label2));
  WaitForResult();
}

}  // namespace content
