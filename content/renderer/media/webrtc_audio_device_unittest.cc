// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/render_thread_impl.h"
#include "content/test/webrtc_audio_device_test.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_hardware_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/voice_engine/include/voe_audio_processing.h"
#include "third_party/webrtc/voice_engine/include/voe_base.h"
#include "third_party/webrtc/voice_engine/include/voe_external_media.h"
#include "third_party/webrtc/voice_engine/include/voe_file.h"
#include "third_party/webrtc/voice_engine/include/voe_network.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using media::AudioParameters;
using testing::_;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrEq;

namespace content {

namespace {

const int kRenderViewId = 1;

scoped_ptr<media::AudioHardwareConfig> CreateRealHardwareConfig(
    media::AudioManager* manager) {
  const AudioParameters output_parameters =
      manager->GetDefaultOutputStreamParameters();
  const AudioParameters input_parameters =
      manager->GetInputStreamParameters(
          media::AudioManagerBase::kDefaultDeviceId);
  return make_scoped_ptr(new media::AudioHardwareConfig(
      input_parameters, output_parameters));
}

// Return true if at least one element in the array matches |value|.
bool FindElementInArray(const int* array, int size, int value) {
  return (std::find(&array[0], &array[0] + size, value) != &array[size]);
}

// This method returns false if a non-supported rate is detected on the
// input or output side.
// TODO(henrika): add support for automatic fallback to Windows Wave audio
// if a non-supported rate is detected. It is probably better to detect
// invalid audio settings by actually trying to open the audio streams instead
// of relying on hard coded conditions.
bool HardwareSampleRatesAreValid() {
  // These are the currently supported hardware sample rates in both directions.
  // The actual WebRTC client can limit these ranges further depending on
  // platform but this is the maximum range we support today.
  int valid_input_rates[] = {16000, 32000, 44100, 48000, 96000};
  int valid_output_rates[] = {16000, 32000, 44100, 48000, 96000};

  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();

  // Verify the input sample rate.
  int input_sample_rate = hardware_config->GetInputSampleRate();

  if (!FindElementInArray(valid_input_rates, arraysize(valid_input_rates),
                          input_sample_rate)) {
    LOG(WARNING) << "Non-supported input sample rate detected.";
    return false;
  }

  // Given that the input rate was OK, verify the output rate as well.
  int output_sample_rate = hardware_config->GetOutputSampleRate();
  if (!FindElementInArray(valid_output_rates, arraysize(valid_output_rates),
                          output_sample_rate)) {
    LOG(WARNING) << "Non-supported output sample rate detected.";
    return false;
  }

  return true;
}

// Utility method which initializes the audio capturer contained in the
// WebRTC audio device. This method should be used in tests where
// HardwareSampleRatesAreValid() has been called and returned true.
bool InitializeCapturer(WebRtcAudioDeviceImpl* webrtc_audio_device) {
  // Access the capturer owned and created by the audio device.
  WebRtcAudioCapturer* capturer = webrtc_audio_device->capturer();
  if (!capturer)
    return false;

  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();

  // Use native capture sample rate and channel configuration to get some
  // action in this test.
  int sample_rate = hardware_config->GetInputSampleRate();
  media::ChannelLayout channel_layout =
      hardware_config->GetInputChannelLayout();
  if (!capturer->Initialize(channel_layout, sample_rate, 1))
    return false;

  return true;
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
                       const bool is_stereo) OVERRIDE {
    base::AutoLock auto_lock(lock_);
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

  int channel_id() const {
    base::AutoLock auto_lock(lock_);
    return channel_id_;
  }

  int type() const {
    base::AutoLock auto_lock(lock_);
    return type_;
  }

  int packet_size() const {
    base::AutoLock auto_lock(lock_);
    return packet_size_;
  }

  int sample_rate() const {
    base::AutoLock auto_lock(lock_);
    return sample_rate_;
  }

 private:
  base::WaitableEvent* event_;
  int channel_id_;
  webrtc::ProcessingTypes type_;
  int packet_size_;
  int sample_rate_;
  int channels_;
  mutable base::Lock lock_;
  DISALLOW_COPY_AND_ASSIGN(WebRTCMediaProcessImpl);
};

}  // end namespace

// Trivial test which verifies that one part of the test harness
// (HardwareSampleRatesAreValid()) works as intended for all supported
// hardware input sample rates.
TEST_F(WebRTCAudioDeviceTest, TestValidInputRates) {
  int valid_rates[] = {16000, 32000, 44100, 48000, 96000};

  // Verify that we will approve all rates listed in |valid_rates|.
  for (size_t i = 0; i < arraysize(valid_rates); ++i) {
    EXPECT_TRUE(FindElementInArray(valid_rates, arraysize(valid_rates),
        valid_rates[i]));
  }

  // Verify that any value outside the valid range results in negative
  // find results.
  int invalid_rates[] = {-1, 0, 8000, 11025, 22050, 192000};
  for (size_t i = 0; i < arraysize(invalid_rates); ++i) {
    EXPECT_FALSE(FindElementInArray(valid_rates, arraysize(valid_rates),
        invalid_rates[i]));
  }
}

// Trivial test which verifies that one part of the test harness
// (HardwareSampleRatesAreValid()) works as intended for all supported
// hardware output sample rates.
TEST_F(WebRTCAudioDeviceTest, TestValidOutputRates) {
  int valid_rates[] = {44100, 48000, 96000};

  // Verify that we will approve all rates listed in |valid_rates|.
  for (size_t i = 0; i < arraysize(valid_rates); ++i) {
    EXPECT_TRUE(FindElementInArray(valid_rates, arraysize(valid_rates),
        valid_rates[i]));
  }

  // Verify that any value outside the valid range results in negative
  // find results.
  int invalid_rates[] = {-1, 0, 8000, 11025, 22050, 32000, 192000};
  for (size_t i = 0; i < arraysize(invalid_rates); ++i) {
    EXPECT_FALSE(FindElementInArray(valid_rates, arraysize(valid_rates),
        invalid_rates[i]));
  }
}

// Basic test that instantiates and initializes an instance of
// WebRtcAudioDeviceImpl.
TEST_F(WebRTCAudioDeviceTest, Construct) {
#if defined(OS_WIN)
  // This test crashes on Win XP bots.
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  AudioParameters input_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_MONO,
      48000,
      16,
      480);

  AudioParameters output_params(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO,
      48000,
      16,
      480);

  media::AudioHardwareConfig audio_config(input_params, output_params);
  SetAudioHardwareConfig(&audio_config);

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());

  // The capturer is not created until after the WebRtcAudioDeviceImpl has
  // been initialized.
  EXPECT_FALSE(InitializeCapturer(webrtc_audio_device.get()));

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  int err = base->Init(webrtc_audio_device);
  EXPECT_TRUE(InitializeCapturer(webrtc_audio_device.get()));
  EXPECT_EQ(0, err);
  EXPECT_EQ(0, base->Terminate());
}

// Verify that a call to webrtc::VoEBase::StartPlayout() starts audio output
// with the correct set of parameters. A WebRtcAudioDeviceImpl instance will
// be utilized to implement the actual audio path. The test registers a
// webrtc::VoEExternalMedia implementation to hijack the output audio and
// verify that streaming starts correctly.
// Disabled when running headless since the bots don't have the required config.
// Flaky, http://crbug.com/167299 .
TEST_F(WebRTCAudioDeviceTest, DISABLED_StartPlayout) {
  if (!has_output_devices_) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true)).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(AnyNumber());

  scoped_refptr<WebRtcAudioRenderer> renderer =
      new WebRtcAudioRenderer(kRenderViewId);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer));

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device);
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
  renderer->Play();

  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));
  WaitForIOThreadCompletion();

  EXPECT_TRUE(webrtc_audio_device->Playing());
  EXPECT_FALSE(webrtc_audio_device->Recording());
  EXPECT_EQ(ch, media_process->channel_id());
  EXPECT_EQ(webrtc::kPlaybackPerChannel, media_process->type());
  EXPECT_EQ(80, media_process->packet_size());
  EXPECT_EQ(8000, media_process->sample_rate());

  EXPECT_EQ(0, external_media->DeRegisterExternalMediaProcessing(
      ch, webrtc::kPlaybackPerChannel));
  EXPECT_EQ(0, base->StopPlayout(ch));
  renderer->Stop();

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

// TODO(leozwang): Because ExternalMediaProcessing is disabled in webrtc,
// disable this unit test on Android for now.
#if defined(OS_ANDROID)
#define MAYBE_StartRecording DISABLED_StartRecording
#else
#define MAYBE_StartRecording StartRecording
#endif
TEST_F(WebRTCAudioDeviceTest, MAYBE_StartRecording) {
  if (!has_input_devices_ || !has_output_devices_) {
    LOG(WARNING) << "Missing audio devices.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  // TODO(tommi): extend MediaObserver and MockMediaObserver with support
  // for new interfaces, like OnSetAudioStreamRecording(). When done, add
  // EXPECT_CALL() macros here.
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device);
  ASSERT_EQ(0, err);

  EXPECT_TRUE(InitializeCapturer(webrtc_audio_device.get()));
  webrtc_audio_device->capturer()->Start();

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

  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));
  WaitForIOThreadCompletion();

  EXPECT_FALSE(webrtc_audio_device->Playing());
  EXPECT_TRUE(webrtc_audio_device->Recording());
  EXPECT_EQ(ch, media_process->channel_id());
  EXPECT_EQ(webrtc::kRecordingPerChannel, media_process->type());
  EXPECT_EQ(80, media_process->packet_size());
  EXPECT_EQ(8000, media_process->sample_rate());

  EXPECT_EQ(0, external_media->DeRegisterExternalMediaProcessing(
      ch, webrtc::kRecordingPerChannel));
  EXPECT_EQ(0, base->StopSend(ch));

  webrtc_audio_device->capturer()->Stop();
  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

// Uses WebRtcAudioDeviceImpl to play a local wave file.
// Disabled when running headless since the bots don't have the required config.
// Flaky, http://crbug.com/167298 .
TEST_F(WebRTCAudioDeviceTest, DISABLED_PlayLocalFile) {
  if (!has_output_devices_) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  std::string file_path(
      GetTestDataPath(FILE_PATH_LITERAL("speechmusic_mono_16kHz.pcm")));

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true)).Times(1);
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed"))).Times(1);
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(AnyNumber());

  scoped_refptr<WebRtcAudioRenderer> renderer =
      new WebRtcAudioRenderer(kRenderViewId);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer));

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device);
  ASSERT_EQ(0, err);

  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);
  EXPECT_EQ(0, base->StartPlayout(ch));
  renderer->Play();

  ScopedWebRTCPtr<webrtc::VoEFile> file(engine.get());
  ASSERT_TRUE(file.valid());
  int duration = 0;
  EXPECT_EQ(0, file->GetFileDuration(file_path.c_str(), duration,
                                     webrtc::kFileFormatPcm16kHzFile));
  EXPECT_NE(0, duration);

  EXPECT_EQ(0, file->StartPlayingFileLocally(ch, file_path.c_str(), false,
                                             webrtc::kFileFormatPcm16kHzFile));

  // Play 2 seconds worth of audio and then quit.
  message_loop_.PostDelayedTask(FROM_HERE,
                                MessageLoop::QuitClosure(),
                                base::TimeDelta::FromSeconds(6));
  message_loop_.Run();

  renderer->Stop();
  EXPECT_EQ(0, base->StopSend(ch));
  EXPECT_EQ(0, base->StopPlayout(ch));
  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

// Uses WebRtcAudioDeviceImpl to play out recorded audio in loopback.
// An external transport implementation is utilized to feed back RTP packets
// which are recorded, encoded, packetized into RTP packets and finally
// "transmitted". The RTP packets are then fed back into the VoiceEngine
// where they are decoded and played out on the default audio output device.
// Disabled when running headless since the bots don't have the required config.
// TODO(henrika): improve quality by using a wideband codec, enabling noise-
// suppressions etc.
// FullDuplexAudioWithAGC is flaky on Android, disable it for now.
#if defined(OS_ANDROID)
#define MAYBE_FullDuplexAudioWithAGC DISABLED_FullDuplexAudioWithAGC
#else
#define MAYBE_FullDuplexAudioWithAGC FullDuplexAudioWithAGC
#endif
TEST_F(WebRTCAudioDeviceTest, MAYBE_FullDuplexAudioWithAGC) {
  if (!has_output_devices_ || !has_input_devices_) {
    LOG(WARNING) << "Missing audio devices.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("created")));
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamPlaying(_, 1, true));
  EXPECT_CALL(media_observer(),
      OnSetAudioStreamStatus(_, 1, StrEq("closed")));
  EXPECT_CALL(media_observer(),
      OnDeleteAudioStream(_, 1)).Times(AnyNumber());

  scoped_refptr<WebRtcAudioRenderer> renderer =
      new WebRtcAudioRenderer(kRenderViewId);
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer));

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device);
  ASSERT_EQ(0, err);

  EXPECT_TRUE(InitializeCapturer(webrtc_audio_device.get()));
  webrtc_audio_device->capturer()->Start();

  ScopedWebRTCPtr<webrtc::VoEAudioProcessing> audio_processing(engine.get());
  ASSERT_TRUE(audio_processing.valid());
#if defined(OS_ANDROID)
  // On Android, by default AGC is off.
  bool enabled = true;
  webrtc::AgcModes agc_mode = webrtc::kAgcDefault;
  EXPECT_EQ(0, audio_processing->GetAgcStatus(enabled, agc_mode));
  EXPECT_FALSE(enabled);
#else
  bool enabled = false;
  webrtc::AgcModes agc_mode = webrtc::kAgcDefault;
  EXPECT_EQ(0, audio_processing->GetAgcStatus(enabled, agc_mode));
  EXPECT_TRUE(enabled);
  EXPECT_EQ(agc_mode, webrtc::kAgcAdaptiveAnalog);
#endif

  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);

  ScopedWebRTCPtr<webrtc::VoENetwork> network(engine.get());
  ASSERT_TRUE(network.valid());
  scoped_ptr<WebRTCTransportImpl> transport(
      new WebRTCTransportImpl(network.get()));
  EXPECT_EQ(0, network->RegisterExternalTransport(ch, *transport.get()));
  EXPECT_EQ(0, base->StartPlayout(ch));
  EXPECT_EQ(0, base->StartSend(ch));
  renderer->Play();

  LOG(INFO) << ">> You should now be able to hear yourself in loopback...";
  message_loop_.PostDelayedTask(FROM_HERE,
                                MessageLoop::QuitClosure(),
                                base::TimeDelta::FromSeconds(2));
  message_loop_.Run();

  webrtc_audio_device->capturer()->Stop();
  renderer->Stop();
  EXPECT_EQ(0, base->StopSend(ch));
  EXPECT_EQ(0, base->StopPlayout(ch));

  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

}  // namespace content
