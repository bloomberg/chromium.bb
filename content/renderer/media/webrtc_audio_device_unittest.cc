// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/test/webrtc_audio_device_test.h"
#include "media/audio/audio_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_audio_processing.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_base.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_file.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_network.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrEq;

namespace {

ACTION_P(QuitMessageLoop, loop_or_proxy) {
  loop_or_proxy->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

class AudioUtil : public AudioUtilInterface {
 public:
  AudioUtil() {}

  virtual double GetAudioHardwareSampleRate() OVERRIDE {
    return media::GetAudioHardwareSampleRate();
  }
  virtual double GetAudioInputHardwareSampleRate() OVERRIDE {
    return media::GetAudioInputHardwareSampleRate();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(AudioUtil);
};

class AudioUtilNoHardware : public AudioUtilInterface {
 public:
  AudioUtilNoHardware(double output_rate, double input_rate)
    : output_rate_(output_rate), input_rate_(input_rate) {
  }

  virtual double GetAudioHardwareSampleRate() OVERRIDE {
    return output_rate_;
  }
  virtual double GetAudioInputHardwareSampleRate() OVERRIDE {
    return input_rate_;
  }

 private:
  double output_rate_;
  double input_rate_;
  DISALLOW_COPY_AND_ASSIGN(AudioUtilNoHardware);
};

bool IsRunningHeadless() {
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar("CHROME_HEADLESS"))
    return true;
  return false;
}

}  // end namespace

// Basic test that instantiates and initializes an instance of
// WebRtcAudioDeviceImpl.
TEST_F(WebRTCAudioDeviceTest, Construct) {
  AudioUtilNoHardware audio_util(48000.0, 48000.0);
  set_audio_util_callback(&audio_util);
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device(
      new WebRtcAudioDeviceImpl());
  audio_device->SetSessionId(1);

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  int err = base->Init(audio_device);
  EXPECT_EQ(0, err);
  EXPECT_EQ(0, base->Terminate());
}

// Uses WebRtcAudioDeviceImpl to play a local wave file.
// Disabled when running headless since the bots don't have the required config.
TEST_F(WebRTCAudioDeviceTest, PlayLocalFile) {
  if (IsRunningHeadless())
    return;

  std::string file_path(
      GetTestDataPath(FILE_PATH_LITERAL("speechmusic_mono_16kHz.pcm")));

  AudioUtil audio_util;
  set_audio_util_callback(&audio_util);

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true)).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(1);

  scoped_refptr<WebRtcAudioDeviceImpl> audio_device(
      new WebRtcAudioDeviceImpl());
  audio_device->SetSessionId(1);

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(audio_device);
  ASSERT_EQ(0, err);

  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);
  EXPECT_EQ(0, base->StartPlayout(ch));

  ScopedWebRTCPtr<webrtc::VoEFile> file(engine.get());
  int duration = 0;
  EXPECT_EQ(0, file->GetFileDuration(file_path.c_str(), duration,
                                     webrtc::kFileFormatPcm16kHzFile));
  EXPECT_NE(0, duration);

  EXPECT_EQ(0, file->StartPlayingFileLocally(ch, file_path.c_str(), false,
      webrtc::kFileFormatPcm16kHzFile));

  message_loop_.PostDelayedTask(FROM_HERE,
                                new MessageLoop::QuitTask(),
                                TestTimeouts::action_timeout_ms());
  message_loop_.Run();

  EXPECT_EQ(0, base->Terminate());
}
