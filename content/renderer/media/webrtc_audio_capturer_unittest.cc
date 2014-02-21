// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"

using ::testing::_;
using ::testing::AtLeast;

namespace content {

namespace {

class MockCapturerSource : public media::AudioCapturerSource {
 public:
  MockCapturerSource() {}
  MOCK_METHOD3(Initialize, void(const media::AudioParameters& params,
                                CaptureCallback* callback,
                                int session_id));
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD1(SetAutomaticGainControl, void(bool enable));

 protected:
  virtual ~MockCapturerSource() {}
};

class MockPeerConnectionAudioSink : public PeerConnectionAudioSink {
 public:
  MockPeerConnectionAudioSink() {}
  ~MockPeerConnectionAudioSink() {}
  MOCK_METHOD9(OnData, int(const int16* audio_data,
                           int sample_rate,
                           int number_of_channels,
                           int number_of_frames,
                           const std::vector<int>& channels,
                           int audio_delay_milliseconds,
                           int current_volume,
                           bool need_audio_processing,
                           bool key_pressed));
  MOCK_METHOD1(OnSetFormat, void(const media::AudioParameters& params));
};

}  // namespace

class WebRtcAudioCapturerTest : public testing::Test {
 protected:
  WebRtcAudioCapturerTest()
#if defined(OS_ANDROID)
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 960) {
    // Android works with a buffer size bigger than 20ms.
#else
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 128) {
#endif
    blink::WebMediaConstraints constraints;
    capturer_ = WebRtcAudioCapturer::CreateCapturer(
        -1, StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE,
                             "", "", params_.sample_rate(),
                             params_.channel_layout(),
                             params_.frames_per_buffer()),
        constraints,
        NULL);
    capturer_source_ = new MockCapturerSource();
    EXPECT_CALL(*capturer_source_.get(), Initialize(_, capturer_.get(), -1));
    capturer_->SetCapturerSourceForTesting(capturer_source_, params_);

    EXPECT_CALL(*capturer_source_.get(), SetAutomaticGainControl(true));
    EXPECT_CALL(*capturer_source_.get(), Start());
    scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
        WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
    track_.reset(new WebRtcLocalAudioTrack(adapter, capturer_, NULL));
    static_cast<WebRtcLocalAudioSourceProvider*>(
        track_->audio_source_provider())->SetSinkParamsForTesting(params_);
    track_->Start();
  }

  media::AudioParameters params_;
  scoped_refptr<MockCapturerSource> capturer_source_;
  scoped_refptr<WebRtcAudioCapturer> capturer_;
  scoped_ptr<WebRtcLocalAudioTrack> track_;
};

// Pass the delay value, volume and key_pressed info via capture callback, and
// those values should be correctly stored and passed to the track.
TEST_F(WebRtcAudioCapturerTest, VerifyAudioParams) {
  // Connect a mock sink to the track.
  scoped_ptr<MockPeerConnectionAudioSink> sink(
      new MockPeerConnectionAudioSink());
  track_->AddSink(sink.get());

  int delay_ms = 65;
  bool key_pressed = true;
  double volume = 0.9;
  // MaxVolume() in WebRtcAudioCapturer is hard-coded to return 255, we add 0.5
  // to do the correct truncation as how the production code does.
  int expected_volume_value = volume * capturer_->MaxVolume() + 0.5;
  scoped_ptr<media::AudioBus> audio_bus = media::AudioBus::Create(params_);
  audio_bus->Zero();
#if defined(OS_ANDROID)
  const int expected_buffer_size = params_.sample_rate() / 100;
#else
  const int expected_buffer_size = params_.frames_per_buffer();
#endif
  bool expected_need_audio_processing = true;
  media::AudioCapturerSource::CaptureCallback* callback =
      static_cast<media::AudioCapturerSource::CaptureCallback*>(capturer_);
  // Verify the sink is getting the correct values.
  EXPECT_CALL(*sink, OnSetFormat(_));
  EXPECT_CALL(*sink,
              OnData(_, params_.sample_rate(), params_.channels(),
                     expected_buffer_size, _, delay_ms,
                     expected_volume_value, expected_need_audio_processing,
                     key_pressed)).Times(AtLeast(1));
  callback->Capture(audio_bus.get(), delay_ms, volume, key_pressed);

  // Verify the cached values in the capturer fits what we expect.
  base::TimeDelta cached_delay;
  int cached_volume = !expected_volume_value;
  bool cached_key_pressed = !key_pressed;
  capturer_->GetAudioProcessingParams(&cached_delay, &cached_volume,
                                      &cached_key_pressed);
  EXPECT_EQ(cached_delay.InMilliseconds(), delay_ms);
  EXPECT_EQ(cached_volume, expected_volume_value);
  EXPECT_EQ(cached_key_pressed, key_pressed);

  track_->RemoveSink(sink.get());
  EXPECT_CALL(*capturer_source_.get(), Stop());
  capturer_->Stop();
}

}  // namespace content
