// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "content/renderer/media/media_stream_audio_level_calculator.h"
#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"
#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace content {

namespace {

class MockWebRtcAudioSink : public webrtc::AudioTrackSinkInterface {
 public:
  MockWebRtcAudioSink() {}
  ~MockWebRtcAudioSink() {}
  MOCK_METHOD5(OnData, void(const void* audio_data,
                            int bits_per_sample,
                            int sample_rate,
                            size_t number_of_channels,
                            size_t number_of_frames));
};

}  // namespace

class WebRtcLocalAudioTrackAdapterTest : public ::testing::Test {
 public:
  WebRtcLocalAudioTrackAdapterTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 480),
        adapter_(WebRtcLocalAudioTrackAdapter::Create(std::string(), NULL)) {
    track_.reset(new WebRtcLocalAudioTrack(adapter_.get()));
  }

 protected:
  void SetUp() override {
    track_->OnSetFormat(params_);
    EXPECT_TRUE(track_->GetAudioAdapter()->enabled());
  }

  media::AudioParameters params_;
  scoped_refptr<WebRtcLocalAudioTrackAdapter> adapter_;
  scoped_ptr<WebRtcLocalAudioTrack> track_;
};

// Adds and Removes a WebRtcAudioSink to a local audio track.
TEST_F(WebRtcLocalAudioTrackAdapterTest, AddAndRemoveSink) {
  // Add a sink to the webrtc track.
  scoped_ptr<MockWebRtcAudioSink> sink(new MockWebRtcAudioSink());
  webrtc::AudioTrackInterface* webrtc_track =
      static_cast<webrtc::AudioTrackInterface*>(adapter_.get());
  webrtc_track->AddSink(sink.get());

  // Send a packet via |track_| and the data should reach the sink of the
  // |adapter_|.
  const scoped_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(params_);
  // While this test is not checking the signal data being passed around, the
  // implementation in WebRtcLocalAudioTrack reads the data for its signal level
  // computation.  Initialize all samples to zero to make the memory sanitizer
  // happy.
  audio_bus->Zero();

  base::TimeTicks estimated_capture_time = base::TimeTicks::Now();
  EXPECT_CALL(*sink,
              OnData(_, 16, params_.sample_rate(), params_.channels(),
                     params_.frames_per_buffer()));
  track_->Capture(*audio_bus, estimated_capture_time);

  // Remove the sink from the webrtc track.
  webrtc_track->RemoveSink(sink.get());
  sink.reset();

  // Verify that no more callback gets into the sink.
  estimated_capture_time +=
      params_.frames_per_buffer() * base::TimeDelta::FromSeconds(1) /
          params_.sample_rate();
  track_->Capture(*audio_bus, estimated_capture_time);
}

TEST_F(WebRtcLocalAudioTrackAdapterTest, GetSignalLevel) {
  webrtc::AudioTrackInterface* webrtc_track =
      static_cast<webrtc::AudioTrackInterface*>(adapter_.get());
  int signal_level = -1;
  EXPECT_FALSE(webrtc_track->GetSignalLevel(&signal_level));
  MediaStreamAudioLevelCalculator calculator;
  adapter_->SetLevel(calculator.level());
  signal_level = -1;
  EXPECT_TRUE(webrtc_track->GetSignalLevel(&signal_level));
  EXPECT_EQ(0, signal_level);
}

}  // namespace content
