// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "content/renderer/media/webrtc_audio_renderer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/render_thread_impl.h"
#include "content/test/webrtc_audio_device_test.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_hardware_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/webrtc/voice_engine/include/voe_audio_processing.h"
#include "third_party/webrtc/voice_engine/include/voe_base.h"
#include "third_party/webrtc/voice_engine/include/voe_codec.h"
#include "third_party/webrtc/voice_engine/include/voe_external_media.h"
#include "third_party/webrtc/voice_engine/include/voe_file.h"
#include "third_party/webrtc/voice_engine/include/voe_network.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using media::AudioParameters;
using media::CHANNEL_LAYOUT_STEREO;
using testing::_;
using testing::AnyNumber;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrEq;

namespace content {

namespace {

const int kRenderViewId = 1;

// The number of packers that RunWebRtcLoopbackTimeTest() uses for measurement.
const int kNumberOfPacketsForLoopbackTest = 100;

// The hardware latency we feed to WebRtc.
const int kHardwareLatencyInMs = 50;

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

// Utility method which creates the audio capturer, it returns a scoped
// reference of the capturer if it is created successfully, otherwise it returns
// NULL. This method should be used in tests where
// HardwareSampleRatesAreValid() has been called and returned true.
scoped_refptr<WebRtcAudioCapturer> CreateAudioCapturer(
    WebRtcAudioDeviceImpl* webrtc_audio_device) {
  media::AudioHardwareConfig* hardware_config =
      RenderThreadImpl::current()->GetAudioHardwareConfig();
  // Use native capture sample rate and channel configuration to get some
  // action in this test.
  int sample_rate = hardware_config->GetInputSampleRate();
  media::ChannelLayout channel_layout =
      hardware_config->GetInputChannelLayout();
  blink::WebMediaConstraints constraints;
  StreamDeviceInfo device(MEDIA_DEVICE_AUDIO_CAPTURE,
                          media::AudioManagerBase::kDefaultDeviceName,
                          media::AudioManagerBase::kDefaultDeviceId,
                          sample_rate, channel_layout, 0);
  device.session_id = 1;
  return WebRtcAudioCapturer::CreateCapturer(kRenderViewId, device,
                                             constraints,
                                             webrtc_audio_device);
}

// Create and start a local audio track. Starting the audio track will connect
// the audio track to the capturer and also start the source of the capturer.
// Also, connect the sink to the audio track.
scoped_ptr<WebRtcLocalAudioTrack>
CreateAndStartLocalAudioTrack(WebRtcLocalAudioTrackAdapter* adapter,
                              WebRtcAudioCapturer* capturer,
                              PeerConnectionAudioSink* sink) {
  scoped_ptr<WebRtcLocalAudioTrack> local_audio_track(
      new WebRtcLocalAudioTrack(adapter, capturer, NULL));

  local_audio_track->AddSink(sink);
  local_audio_track->Start();
  return local_audio_track.Pass();
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
  virtual void Process(int channel,
                       webrtc::ProcessingTypes type,
                       int16_t audio_10ms[],
                       int length,
                       int sampling_freq,
                       bool is_stereo) OVERRIDE {
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

// TODO(xians): Use MediaStreamAudioSink.
class MockMediaStreamAudioSink : public PeerConnectionAudioSink {
 public:
  explicit MockMediaStreamAudioSink(base::WaitableEvent* event)
      : event_(event) {
    DCHECK(event_);
  }
  virtual ~MockMediaStreamAudioSink() {}

  // PeerConnectionAudioSink implementation.
  virtual int OnData(const int16* audio_data,
                     int sample_rate,
                     int number_of_channels,
                     int number_of_frames,
                     const std::vector<int>& channels,
                     int audio_delay_milliseconds,
                     int current_volume,
                     bool need_audio_processing,
                     bool key_pressed) OVERRIDE {
    // Signal that a callback has been received.
    event_->Signal();
    return 0;
  }

  // Set the format for the capture audio parameters.
  virtual void OnSetFormat(
      const media::AudioParameters& params) OVERRIDE {}

 private:
   base::WaitableEvent* event_;

   DISALLOW_COPY_AND_ASSIGN(MockMediaStreamAudioSink);
};

class MockWebRtcAudioRendererSource : public WebRtcAudioRendererSource {
 public:
  explicit MockWebRtcAudioRendererSource(base::WaitableEvent* event)
      : event_(event) {
    DCHECK(event_);
  }
  virtual ~MockWebRtcAudioRendererSource() {}

  // WebRtcAudioRendererSource implementation.
  virtual void RenderData(media::AudioBus* audio_bus,
                          int sample_rate,
                          int audio_delay_milliseconds) OVERRIDE {
    // Signal that a callback has been received.
    // Initialize the memory to zero to avoid uninitialized warning from
    // Valgrind.
    audio_bus->Zero();
    event_->Signal();
  }

  virtual void RemoveAudioRenderer(WebRtcAudioRenderer* renderer) OVERRIDE {};

 private:
   base::WaitableEvent* event_;

   DISALLOW_COPY_AND_ASSIGN(MockWebRtcAudioRendererSource);
};

// Prints numerical information to stdout in a controlled format so we can plot
// the result.
void PrintPerfResultMs(const char* graph, const char* trace, float time_ms) {
  std::string times;
  base::StringAppendF(&times, "%.2f,", time_ms);
  std::string result = base::StringPrintf(
      "%sRESULT %s%s: %s= %s%s%s %s\n", "*", graph, "",
      trace,  "[", times.c_str(), "]", "ms");

  fflush(stdout);
  printf("%s", result.c_str());
  fflush(stdout);
}

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath data_file;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &data_file));
  data_file =
      data_file.Append(FILE_PATH_LITERAL("media"))
               .Append(FILE_PATH_LITERAL("test"))
               .Append(FILE_PATH_LITERAL("data"))
               .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(data_file));
  int64 data_file_size64 = 0;
  DCHECK(base::GetFileSize(data_file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(data_file, data, length));
  DCHECK(data_file_size64 > length);
}

void SetChannelCodec(webrtc::VoiceEngine* engine, int channel) {
  // TODO(xians): move the codec as an input param to this function, and add
  // tests for different codecs, also add support to Android and IOS.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  webrtc::CodecInst isac;
  strcpy(isac.plname, "ISAC");
  isac.pltype = 104;
  isac.pacsize = 960;
  isac.plfreq = 32000;
  isac.channels = 1;
  isac.rate = -1;
  ScopedWebRTCPtr<webrtc::VoECodec> codec(engine);
  EXPECT_EQ(0, codec->SetRecPayloadType(channel, isac));
  EXPECT_EQ(0, codec->SetSendCodec(channel, isac));
#endif
}

// Returns the time in millisecond for sending packets to WebRtc for encoding,
// signal processing, decoding and receiving them back.
int RunWebRtcLoopbackTimeTest(media::AudioManager* manager,
                              bool enable_apm) {
  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  EXPECT_TRUE(engine.valid());
  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  EXPECT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device.get());
  EXPECT_EQ(0, err);

  // We use OnSetFormat() to configure the audio parameters so that this
  // test can run on machine without hardware device.
  const media::AudioParameters params = media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, CHANNEL_LAYOUT_STEREO,
      48000, 2, 480);
  PeerConnectionAudioSink* capturer_sink =
      static_cast<PeerConnectionAudioSink*>(webrtc_audio_device.get());
  WebRtcAudioRendererSource* renderer_source =
      static_cast<WebRtcAudioRendererSource*>(webrtc_audio_device.get());

  // Turn on/off all the signal processing components like AGC, AEC and NS.
  ScopedWebRTCPtr<webrtc::VoEAudioProcessing> audio_processing(engine.get());
  EXPECT_TRUE(audio_processing.valid());
  audio_processing->SetAgcStatus(enable_apm);
  audio_processing->SetNsStatus(enable_apm);
  audio_processing->SetEcStatus(enable_apm);

  // Create a voice channel for the WebRtc.
  int channel = base->CreateChannel();
  EXPECT_NE(-1, channel);
  SetChannelCodec(engine.get(), channel);

  // Use our fake network transmission and start playout and recording.
  ScopedWebRTCPtr<webrtc::VoENetwork> network(engine.get());
  EXPECT_TRUE(network.valid());
  scoped_ptr<WebRTCTransportImpl> transport(
      new WebRTCTransportImpl(network.get()));
  EXPECT_EQ(0, network->RegisterExternalTransport(channel, *transport.get()));
  EXPECT_EQ(0, base->StartPlayout(channel));
  EXPECT_EQ(0, base->StartSend(channel));

  // Read speech data from a speech test file.
  const int input_packet_size =
      params.frames_per_buffer() * 2 * params.channels();
  const size_t length = input_packet_size * kNumberOfPacketsForLoopbackTest;
  scoped_ptr<char[]> capture_data(new char[length]);
  ReadDataFromSpeechFile(capture_data.get(), length);

  // Start the timer.
  scoped_ptr<media::AudioBus> render_audio_bus(media::AudioBus::Create(params));
  base::Time start_time = base::Time::Now();
  int delay = 0;
  std::vector<int> voe_channels;
  voe_channels.push_back(channel);
  for (int j = 0; j < kNumberOfPacketsForLoopbackTest; ++j) {
    // Sending fake capture data to WebRtc.
    capturer_sink->OnData(
        reinterpret_cast<int16*>(capture_data.get() + input_packet_size * j),
        params.sample_rate(),
        params.channels(),
        params.frames_per_buffer(),
        voe_channels,
        kHardwareLatencyInMs,
        1.0,
        enable_apm,
        false);

    // Receiving data from WebRtc.
    renderer_source->RenderData(
        render_audio_bus.get(), params.sample_rate(),
        kHardwareLatencyInMs + delay);
    delay = (base::Time::Now() - start_time).InMilliseconds();
  }

  int latency = (base::Time::Now() - start_time).InMilliseconds();

  EXPECT_EQ(0, base->StopSend(channel));
  EXPECT_EQ(0, base->StopPlayout(channel));
  EXPECT_EQ(0, base->DeleteChannel(channel));
  EXPECT_EQ(0, base->Terminate());

  return latency;
}

}  // namespace

// Trivial test which verifies that one part of the test harness
// (HardwareSampleRatesAreValid()) works as intended for all supported
// hardware input sample rates.
TEST_F(MAYBE_WebRTCAudioDeviceTest, TestValidInputRates) {
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
TEST_F(MAYBE_WebRTCAudioDeviceTest, TestValidOutputRates) {
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
TEST_F(MAYBE_WebRTCAudioDeviceTest, Construct) {
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

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  int err = base->Init(webrtc_audio_device.get());
  EXPECT_TRUE(CreateAudioCapturer(webrtc_audio_device) != NULL);
  EXPECT_EQ(0, err);
  EXPECT_EQ(0, base->Terminate());
}

// Verify that a call to webrtc::VoEBase::StartPlayout() starts audio output
// with the correct set of parameters. A WebRtcAudioDeviceImpl instance will
// be utilized to implement the actual audio path. The test registers a
// webrtc::VoEExternalMedia implementation to hijack the output audio and
// verify that streaming starts correctly.
// TODO(henrika): include on Android as well as soon as alla race conditions
// in OpenSLES are resolved.
#if defined(OS_ANDROID)
#define MAYBE_StartPlayout DISABLED_StartPlayout
#else
#define MAYBE_StartPlayout StartPlayout
#endif
TEST_F(MAYBE_WebRTCAudioDeviceTest, MAYBE_StartPlayout) {
  if (!has_output_devices_) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());
  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  int err = base->Init(webrtc_audio_device.get());
  ASSERT_EQ(0, err);

  ScopedWebRTCPtr<webrtc::VoEExternalMedia> external_media(engine.get());
  ASSERT_TRUE(external_media.valid());
  base::WaitableEvent event(false, false);
  scoped_ptr<WebRTCMediaProcessImpl> media_process(
      new WebRTCMediaProcessImpl(&event));
  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);
  EXPECT_EQ(0, external_media->RegisterExternalMediaProcessing(
      ch, webrtc::kPlaybackPerChannel, *media_process.get()));

  scoped_refptr<webrtc::MediaStreamInterface> media_stream(
      new talk_base::RefCountedObject<MockMediaStream>("label"));

  EXPECT_EQ(0, base->StartPlayout(ch));
  scoped_refptr<WebRtcAudioRenderer> renderer(
      CreateDefaultWebRtcAudioRenderer(kRenderViewId, media_stream));
  scoped_refptr<MediaStreamAudioRenderer> proxy(
      renderer->CreateSharedAudioRendererProxy(media_stream));
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer.get()));
  proxy->Start();
  proxy->Play();

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
  proxy->Stop();
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
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// This test is failing on ARM linux: http://crbug.com/238490
#define MAYBE_StartRecording DISABLED_StartRecording
#else
// Flakily hangs on all other platforms as well: crbug.com/268376.
// When the flakiness has been fixed, you probably want to leave it disabled
// on the above platforms.
#define MAYBE_StartRecording DISABLED_StartRecording
#endif

TEST_F(MAYBE_WebRTCAudioDeviceTest, MAYBE_StartRecording) {
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
  int err = base->Init(webrtc_audio_device.get());
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

  // Create the capturer which starts the source of the data flow.
  scoped_refptr<WebRtcAudioCapturer> capturer(
      CreateAudioCapturer(webrtc_audio_device));
  EXPECT_TRUE(capturer);

  // Create and start a local audio track which is bridging the data flow
  // between the capturer and WebRtcAudioDeviceImpl.
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> local_audio_track(
      CreateAndStartLocalAudioTrack(adapter, capturer, webrtc_audio_device));
  // connect the VoE voice channel to the audio track
  static_cast<webrtc::AudioTrackInterface*>(
      adapter.get())->GetRenderer()->AddChannel(ch);

  // Verify we get the data flow.
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

  capturer->Stop();
  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

// Uses WebRtcAudioDeviceImpl to play a local wave file.
// TODO(henrika): include on Android as well as soon as alla race conditions
// in OpenSLES are resolved.
#if defined(OS_ANDROID)
#define MAYBE_PlayLocalFile DISABLED_PlayLocalFile
#else
#define MAYBE_PlayLocalFile PlayLocalFile
#endif
TEST_F(MAYBE_WebRTCAudioDeviceTest, MAYBE_PlayLocalFile) {
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

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());
  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  int err = base->Init(webrtc_audio_device.get());
  ASSERT_EQ(0, err);
  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);
  EXPECT_EQ(0, base->StartPlayout(ch));
  scoped_refptr<webrtc::MediaStreamInterface> media_stream(
      new talk_base::RefCountedObject<MockMediaStream>("label"));
  scoped_refptr<WebRtcAudioRenderer> renderer(
      CreateDefaultWebRtcAudioRenderer(kRenderViewId, media_stream));
  scoped_refptr<MediaStreamAudioRenderer> proxy(
      renderer->CreateSharedAudioRendererProxy(media_stream));
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer.get()));
  proxy->Start();
  proxy->Play();

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
                                base::MessageLoop::QuitClosure(),
                                base::TimeDelta::FromSeconds(2));
  message_loop_.Run();

  proxy->Stop();
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
// Also flakily hangs on Windows: crbug.com/269348.
#if defined(OS_ANDROID) || defined(OS_WIN)
#define MAYBE_FullDuplexAudioWithAGC DISABLED_FullDuplexAudioWithAGC
#else
#define MAYBE_FullDuplexAudioWithAGC FullDuplexAudioWithAGC
#endif
TEST_F(MAYBE_WebRTCAudioDeviceTest, MAYBE_FullDuplexAudioWithAGC) {
  if (!has_output_devices_ || !has_input_devices_) {
    LOG(WARNING) << "Missing audio devices.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());
  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());
  int err = base->Init(webrtc_audio_device.get());
  ASSERT_EQ(0, err);

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

  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_refptr<WebRtcAudioCapturer> capturer(
      CreateAudioCapturer(webrtc_audio_device));
  EXPECT_TRUE(capturer);
  scoped_ptr<WebRtcLocalAudioTrack> local_audio_track(
      CreateAndStartLocalAudioTrack(adapter, capturer, webrtc_audio_device));
  // connect the VoE voice channel to the audio track adapter.
  static_cast<webrtc::AudioTrackInterface*>(
      adapter.get())->GetRenderer()->AddChannel(ch);

  ScopedWebRTCPtr<webrtc::VoENetwork> network(engine.get());
  ASSERT_TRUE(network.valid());
  scoped_ptr<WebRTCTransportImpl> transport(
      new WebRTCTransportImpl(network.get()));
  EXPECT_EQ(0, network->RegisterExternalTransport(ch, *transport.get()));
  EXPECT_EQ(0, base->StartPlayout(ch));
  EXPECT_EQ(0, base->StartSend(ch));
  scoped_refptr<webrtc::MediaStreamInterface> media_stream(
      new talk_base::RefCountedObject<MockMediaStream>("label"));
  scoped_refptr<WebRtcAudioRenderer> renderer(
      CreateDefaultWebRtcAudioRenderer(kRenderViewId, media_stream));
  scoped_refptr<MediaStreamAudioRenderer> proxy(
      renderer->CreateSharedAudioRendererProxy(media_stream));
  EXPECT_TRUE(webrtc_audio_device->SetAudioRenderer(renderer.get()));
  proxy->Start();
  proxy->Play();

  VLOG(0) << ">> You should now be able to hear yourself in loopback...";
  message_loop_.PostDelayedTask(FROM_HERE,
                                base::MessageLoop::QuitClosure(),
                                base::TimeDelta::FromSeconds(2));
  message_loop_.Run();

  capturer->Stop();
  proxy->Stop();
  EXPECT_EQ(0, base->StopSend(ch));
  EXPECT_EQ(0, base->StopPlayout(ch));

  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}

// Test times out on bots, see http://crbug.com/247447
TEST_F(MAYBE_WebRTCAudioDeviceTest, DISABLED_WebRtcRecordingSetupTime) {
  if (!has_input_devices_) {
    LOG(WARNING) << "Missing audio capture devices.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  scoped_refptr<WebRtcAudioDeviceImpl> webrtc_audio_device(
      new WebRtcAudioDeviceImpl());

  WebRTCAutoDelete<webrtc::VoiceEngine> engine(webrtc::VoiceEngine::Create());
  ASSERT_TRUE(engine.valid());

  ScopedWebRTCPtr<webrtc::VoEBase> base(engine.get());
  ASSERT_TRUE(base.valid());
  int err = base->Init(webrtc_audio_device.get());
  ASSERT_EQ(0, err);

  int ch = base->CreateChannel();
  EXPECT_NE(-1, ch);

  scoped_refptr<WebRtcAudioCapturer> capturer(
      CreateAudioCapturer(webrtc_audio_device));
  EXPECT_TRUE(capturer);
  base::WaitableEvent event(false, false);
  scoped_ptr<MockMediaStreamAudioSink> sink(
      new MockMediaStreamAudioSink(&event));

  // Create and start a local audio track. Starting the audio track will connect
  // the audio track to the capturer and also start the source of the capturer.
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
      WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
  scoped_ptr<WebRtcLocalAudioTrack> local_audio_track(
      CreateAndStartLocalAudioTrack(adapter, capturer, sink.get()));

  // connect the VoE voice channel to the audio track adapter.
  static_cast<webrtc::AudioTrackInterface*>(
      adapter.get())->GetRenderer()->AddChannel(ch);

  base::Time start_time = base::Time::Now();
  EXPECT_EQ(0, base->StartSend(ch));

  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));
  int delay = (base::Time::Now() - start_time).InMilliseconds();
  PrintPerfResultMs("webrtc_recording_setup_c", "t", delay);

  capturer->Stop();
  EXPECT_EQ(0, base->StopSend(ch));
  EXPECT_EQ(0, base->DeleteChannel(ch));
  EXPECT_EQ(0, base->Terminate());
}


// TODO(henrika): include on Android as well as soon as alla race conditions
// in OpenSLES are resolved.
#if defined(OS_ANDROID)
#define MAYBE_WebRtcPlayoutSetupTime DISABLED_WebRtcPlayoutSetupTime
#else
#define MAYBE_WebRtcPlayoutSetupTime WebRtcPlayoutSetupTime
#endif
TEST_F(MAYBE_WebRTCAudioDeviceTest, MAYBE_WebRtcPlayoutSetupTime) {
  if (!has_output_devices_) {
    LOG(WARNING) << "No output device detected.";
    return;
  }

  scoped_ptr<media::AudioHardwareConfig> config =
      CreateRealHardwareConfig(audio_manager_.get());
  SetAudioHardwareConfig(config.get());

  if (!HardwareSampleRatesAreValid())
    return;

  base::WaitableEvent event(false, false);
  scoped_ptr<MockWebRtcAudioRendererSource> renderer_source(
      new MockWebRtcAudioRendererSource(&event));

  scoped_refptr<webrtc::MediaStreamInterface> media_stream(
      new talk_base::RefCountedObject<MockMediaStream>("label"));
  scoped_refptr<WebRtcAudioRenderer> renderer(
      CreateDefaultWebRtcAudioRenderer(kRenderViewId, media_stream));
  renderer->Initialize(renderer_source.get());
  scoped_refptr<MediaStreamAudioRenderer> proxy(
      renderer->CreateSharedAudioRendererProxy(media_stream));
  proxy->Start();

  // Start the timer and playout.
  base::Time start_time = base::Time::Now();
  proxy->Play();
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_timeout()));
  int delay = (base::Time::Now() - start_time).InMilliseconds();
  PrintPerfResultMs("webrtc_playout_setup_c", "t", delay);

  proxy->Stop();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_WebRtcLoopbackTimeWithoutSignalProcessing \
        DISABLED_WebRtcLoopbackTimeWithoutSignalProcessing
#else
#define MAYBE_WebRtcLoopbackTimeWithoutSignalProcessing \
        WebRtcLoopbackTimeWithoutSignalProcessing
#endif

TEST_F(MAYBE_WebRTCAudioDeviceTest,
       MAYBE_WebRtcLoopbackTimeWithoutSignalProcessing) {
#if defined(OS_WIN)
  // This test hangs on WinXP: see http://crbug.com/318189.
  if (base::win::GetVersion() <= base::win::VERSION_XP) {
    LOG(WARNING) << "Test disabled due to the test hangs on WinXP.";
    return;
  }
#endif
  int latency = RunWebRtcLoopbackTimeTest(audio_manager_.get(), false);
  PrintPerfResultMs("webrtc_loopback_without_sigal_processing (100 packets)",
                    "t", latency);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_WebRtcLoopbackTimeWithSignalProcessing \
        DISABLED_WebRtcLoopbackTimeWithSignalProcessing
#else
#define MAYBE_WebRtcLoopbackTimeWithSignalProcessing \
        WebRtcLoopbackTimeWithSignalProcessing
#endif

TEST_F(MAYBE_WebRTCAudioDeviceTest,
       MAYBE_WebRtcLoopbackTimeWithSignalProcessing) {
#if defined(OS_WIN)
  // This test hangs on WinXP: see http://crbug.com/318189.
  if (base::win::GetVersion() <= base::win::VERSION_XP) {
    LOG(WARNING) << "Test disabled due to the test hangs on WinXP.";
    return;
  }
#endif
  int latency = RunWebRtcLoopbackTimeTest(audio_manager_.get(), true);
  PrintPerfResultMs("webrtc_loopback_with_signal_processing (100 packets)",
                    "t", latency);
}

}  // namespace content
