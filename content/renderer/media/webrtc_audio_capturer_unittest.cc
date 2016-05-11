// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/renderer/media_stream_audio_sink.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
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
  ~MockCapturerSource() override {}
};

class MockMediaStreamAudioSink : public MediaStreamAudioSink {
 public:
  MockMediaStreamAudioSink() {}
  ~MockMediaStreamAudioSink() override {}
  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) override {
    EXPECT_EQ(audio_bus.channels(), params_.channels());
    EXPECT_EQ(audio_bus.frames(), params_.frames_per_buffer());
    EXPECT_FALSE(estimated_capture_time.is_null());
    OnDataCallback();
  }
  MOCK_METHOD0(OnDataCallback, void());
  void OnSetFormat(const media::AudioParameters& params) override {
    params_ = params;
    FormatIsSet();
  }
  MOCK_METHOD0(FormatIsSet, void());

 private:
  media::AudioParameters params_;
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
  }

  void VerifyAudioParams(const blink::WebMediaConstraints& constraints,
                         bool need_audio_processing) {
    const std::unique_ptr<WebRtcAudioCapturer> capturer =
        WebRtcAudioCapturer::CreateCapturer(
            -1, StreamDeviceInfo(
                    MEDIA_DEVICE_AUDIO_CAPTURE, "", "", params_.sample_rate(),
                    params_.channel_layout(), params_.frames_per_buffer()),
            constraints, nullptr, nullptr);
    const scoped_refptr<MockCapturerSource> capturer_source(
        new MockCapturerSource());
    EXPECT_CALL(*capturer_source.get(), Initialize(_, capturer.get(), -1));
    EXPECT_CALL(*capturer_source.get(), SetAutomaticGainControl(true));
    EXPECT_CALL(*capturer_source.get(), Start());
    capturer->SetCapturerSource(capturer_source, params_);

    scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter(
        WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL));
    const std::unique_ptr<WebRtcLocalAudioTrack> track(
        new WebRtcLocalAudioTrack(adapter.get()));
    capturer->AddTrack(track.get());

    // Connect a mock sink to the track.
    std::unique_ptr<MockMediaStreamAudioSink> sink(
        new MockMediaStreamAudioSink());
    track->AddSink(sink.get());

    int delay_ms = 65;
    bool key_pressed = true;
    double volume = 0.9;

    std::unique_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(params_);
    audio_bus->Zero();

    media::AudioCapturerSource::CaptureCallback* callback =
        static_cast<media::AudioCapturerSource::CaptureCallback*>(
            capturer.get());

    // Verify the sink is getting the correct values.
    EXPECT_CALL(*sink, FormatIsSet());
    EXPECT_CALL(*sink, OnDataCallback()).Times(AtLeast(1));
    callback->Capture(audio_bus.get(), delay_ms, volume, key_pressed);

    track->RemoveSink(sink.get());
    EXPECT_CALL(*capturer_source.get(), Stop());
    capturer->Stop();
  }

  media::AudioParameters params_;
};

TEST_F(WebRtcAudioCapturerTest, VerifyAudioParamsWithAudioProcessing) {
  // Turn off the default constraints to verify that the sink will get packets
  // with a buffer size smaller than 10ms.
  MockConstraintFactory constraint_factory;
  constraint_factory.DisableDefaultAudioConstraints();
  VerifyAudioParams(constraint_factory.CreateWebMediaConstraints(), false);
}

TEST_F(WebRtcAudioCapturerTest, FailToCreateCapturerWithWrongConstraints) {
  MockConstraintFactory constraint_factory;
  const std::string dummy_constraint = "dummy";
  // Set a non-audio constraint.
  constraint_factory.basic().width.setExact(240);

  std::unique_ptr<WebRtcAudioCapturer> capturer(
      WebRtcAudioCapturer::CreateCapturer(
          0, StreamDeviceInfo(MEDIA_DEVICE_AUDIO_CAPTURE, "", "",
                              params_.sample_rate(), params_.channel_layout(),
                              params_.frames_per_buffer()),
          constraint_factory.CreateWebMediaConstraints(), NULL, NULL));
  EXPECT_TRUE(capturer.get() == NULL);
}


}  // namespace content
