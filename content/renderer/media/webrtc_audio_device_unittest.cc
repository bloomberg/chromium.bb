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
#include "third_party/webrtc/voice_engine/main/interface/voe_external_media.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_file.h"
#include "third_party/webrtc/voice_engine/main/interface/voe_network.h"

using testing::_;
using testing::AnyNumber;
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

class WebRTCMediaProcessImpl : public webrtc::VoEMediaProcess {
 public:
  explicit WebRTCMediaProcessImpl(base::WaitableEvent* event)
      : event_(event),
        channel_id_(-1),
        type_(webrtc::kPlaybackPerChannel),
        packet_size_(0),
        sample_rate_(0),
        channels_(0) {
  }
  virtual ~WebRTCMediaProcessImpl() {}

  // TODO(henrika): Refactor in WebRTC and convert to Chrome coding style.
  virtual void Process(const int channel,
                       const webrtc::ProcessingTypes type,
                       WebRtc_Word16 audio_10ms[],
                       const int length,
                       const int sampling_freq,
                       const bool is_stereo) {
    channel_id_ = channel;
    type_ = type;
    packet_size_ = length;
    sample_rate_ = sampling_freq;
    channels_ = (is_stereo ? 2 : 1);
    if (event_) {
      // Signal that a new callback has been received.
      event_->Signal();
    }
  }

  int channel_id() const { return channel_id_; }
  int type() const { return type_; }
  int packet_size() const { return packet_size_; }
  int sample_rate() const { return sample_rate_; }
  int channels() const { return channels_; }

 private:
  base::WaitableEvent* event_;
  int channel_id_;
  webrtc::ProcessingTypes type_;
  int packet_size_;
  int sample_rate_;
  int channels_;
  DISALLOW_COPY_AND_ASSIGN(WebRTCMediaProcessImpl);
};

}  // end namespace

// Basic test that instantiates and initializes an instance of
// WebRtcAudioDeviceImpl.
TEST_F(WebRTCAudioDeviceTest, Construct) {
  AudioUtilNoHardware audio_util(48000.0, 48000.0);
  SetAudioUtilCallback(&audio_util);
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

// Verify that a call to webrtc::VoEBase::StartPlayout() starts audio output
// with the correct set of parameters. A WebRtcAudioDeviceImpl instance will
// be utilized to implement the actual audio path. The test registers a
// webrtc::VoEExternalMedia implementation to hijack the output audio and
// verify that streaming starts correctly.
// Disabled when running headless since the bots don't have the required config.
TEST_F(WebRTCAudioDeviceTest, StartPlayout) {
  if (IsRunningHeadless())
    return;

  AudioUtil audio_util;
  SetAudioUtilCallback(&audio_util);

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true)).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(AnyNumber());

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

  ScopedWebRTCPtr<webrtc::VoEExternalMedia> external_media(engine.get());
  ASSERT_TRUE(external_media.valid());

  base::WaitableEvent event(false, false);
  scoped_ptr<WebRTCMediaProcessImpl> media_process(
      new WebRTCMediaProcessImpl(&event));
  EXPECT_EQ(0, external_media->RegisterExternalMediaProcessing(
      ch, webrtc::kPlaybackPerChannel, *media_process.get()));

  EXPECT_EQ(0, base->StartPlayout(ch));

  EXPECT_TRUE(event.TimedWait(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
  WaitForIOThreadCompletion();

  EXPECT_TRUE(audio_device->playing());
  EXPECT_FALSE(audio_device->recording());
  EXPECT_EQ(ch, media_process->channel_id());
  EXPECT_EQ(webrtc::kPlaybackPerChannel, media_process->type());
  EXPECT_EQ(80, media_process->packet_size());
  EXPECT_EQ(8000, media_process->sample_rate());

  EXPECT_EQ(0, external_media->DeRegisterExternalMediaProcessing(
      ch, webrtc::kPlaybackPerChannel));
  EXPECT_EQ(0, base->StopPlayout(ch));

  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

// Verify that a call to webrtc::VoEBase::StartRecording() starts audio input
// with the correct set of parameters. A WebRtcAudioDeviceImpl instance will
// be utilized to implement the actual audio path. The test registers a
// webrtc::VoEExternalMedia implementation to hijack the input audio and
// verify that streaming starts correctly. An external transport implementation
// is also required to ensure that "sending" can start without actually trying
// to send encoded packets to the network. Our main interest here is to ensure
// that the audio capturing starts as it should.
// Disabled when running headless since the bots don't have the required config.
TEST_F(WebRTCAudioDeviceTest, StartRecording) {
  if (IsRunningHeadless())
    return;

  AudioUtil audio_util;
  SetAudioUtilCallback(&audio_util);

  // TODO(tommi): extend MediaObserver and MockMediaObserver with support
  // for new interfaces, like OnSetAudioStreamRecording(). When done, add
  // EXPECT_CALL() macros here.

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

  ScopedWebRTCPtr<webrtc::VoEExternalMedia> external_media(engine.get());
  ASSERT_TRUE(external_media.valid());

  base::WaitableEvent event(false, false);
  scoped_ptr<WebRTCMediaProcessImpl> media_process(
      new WebRTCMediaProcessImpl(&event));
  EXPECT_EQ(0, external_media->RegisterExternalMediaProcessing(
      ch, webrtc::kRecordingPerChannel, *media_process.get()));

  // We must add an external transport implementation to be able to start
  // recording without actually sending encoded packets to the network. All
  // we want to do here is to verify that audio capturing starts as it should.
  ScopedWebRTCPtr<webrtc::VoENetwork> network(engine.get());
  scoped_ptr<WebRTCTransportImpl> transport(
      new WebRTCTransportImpl(network.get()));
  EXPECT_EQ(0, network->RegisterExternalTransport(ch, *transport.get()));
  EXPECT_EQ(0, base->StartSend(ch));

  EXPECT_TRUE(event.TimedWait(
      base::TimeDelta::FromMilliseconds(TestTimeouts::action_timeout_ms())));
  WaitForIOThreadCompletion();

  EXPECT_FALSE(audio_device->playing());
  EXPECT_TRUE(audio_device->recording());
  EXPECT_EQ(ch, media_process->channel_id());
  EXPECT_EQ(webrtc::kRecordingPerChannel, media_process->type());
  EXPECT_EQ(80, media_process->packet_size());
  EXPECT_EQ(8000, media_process->sample_rate());

  EXPECT_EQ(0, external_media->DeRegisterExternalMediaProcessing(
      ch, webrtc::kRecordingPerChannel));
  EXPECT_EQ(0, base->StopSend(ch));

  EXPECT_EQ(0, base->DeleteChannel(ch));
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
  SetAudioUtilCallback(&audio_util);

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true)).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(AnyNumber());

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

// Uses WebRtcAudioDeviceImpl to play out recorded audio in loopback.
// An external transport implementation is utilized to feed back RTP packets
// which are recorded, encoded, packetized into RTP packets and finally
// "transmitted". The RTP packets are then fed back into the VoiceEngine
// where they are decoded and played out on the default audio output device.
// Disabled when running headless since the bots don't have the required config.
// TODO(henrika): improve quality by using a wideband codec, enabling noise-
// suppressions and perhaps also the digital AGC.
TEST_F(WebRTCAudioDeviceTest, FullDuplexAudio) {
  if (IsRunningHeadless())
    return;

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created")));
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true));
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed")));

  AudioUtil audio_util;
  SetAudioUtilCallback(&audio_util);

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

  ScopedWebRTCPtr<webrtc::VoENetwork> network(engine.get());
  scoped_ptr<WebRTCTransportImpl> transport(
      new WebRTCTransportImpl(network.get()));
  EXPECT_EQ(0, network->RegisterExternalTransport(ch, *transport.get()));
  EXPECT_EQ(0, base->StartPlayout(ch));
  EXPECT_EQ(0, base->StartSend(ch));

  LOG(INFO) << ">> You should now be able to hear yourself in loopback...";
  message_loop_.PostDelayedTask(FROM_HERE,
                                new MessageLoop::QuitTask(),
                                TestTimeouts::action_timeout_ms());
  message_loop_.Run();

  EXPECT_EQ(0, base->StopSend(ch));
  EXPECT_EQ(0, base->StopPlayout(ch));

  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}
