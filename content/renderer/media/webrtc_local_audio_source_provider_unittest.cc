// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/media/webrtc_local_audio_source_provider.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WebRtcLocalAudioSourceProviderTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    source_params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         media::CHANNEL_LAYOUT_MONO, 1, 0, 48000, 16, 480);
    sink_params_.Reset(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO, 2, 0, 44100, 16,
        WebRtcLocalAudioSourceProvider::kWebAudioRenderBufferSize);
    source_bus_ = media::AudioBus::Create(source_params_);
    sink_bus_ = media::AudioBus::Create(sink_params_);
    source_provider_.reset(new WebRtcLocalAudioSourceProvider());
    source_provider_->SetSinkParamsForTesting(sink_params_);
    source_provider_->Initialize(source_params_);
  }

  media::AudioParameters source_params_;
  media::AudioParameters sink_params_;
  scoped_ptr<media::AudioBus> source_bus_;
  scoped_ptr<media::AudioBus> sink_bus_;
  scoped_ptr<WebRtcLocalAudioSourceProvider> source_provider_;
};

TEST_F(WebRtcLocalAudioSourceProviderTest, VerifyDataFlow) {
  // Point the WebVector into memory owned by |sink_bus_|.
  blink::WebVector<float*> audio_data(
      static_cast<size_t>(sink_bus_->channels()));
  for (size_t i = 0; i < audio_data.size(); ++i)
    audio_data[i] = sink_bus_->channel(i);

  // Enable the |source_provider_| by asking for data. This will inject
  // source_params_.frames_per_buffer() of zero into the resampler since there
  // no available data in the FIFO.
  source_provider_->provideInput(audio_data, sink_params_.frames_per_buffer());
  EXPECT_TRUE(sink_bus_->channel(0)[0] == 0);

  // Set the value of source data to be 1.
  for (int i = 0; i < source_params_.frames_per_buffer(); ++i) {
    source_bus_->channel(0)[i] = 1;
  }

  // Deliver data to |source_provider_|.
  source_provider_->DeliverData(source_bus_.get(), 0, 0, false);

  // Consume the first packet in the resampler, which contains only zero.
  // And the consumption of the data will trigger pulling the real packet from
  // the source provider FIFO into the resampler.
  // Note that we need to count in the provideInput() call a few lines above.
  for (int i = sink_params_.frames_per_buffer();
       i < source_params_.frames_per_buffer();
       i += sink_params_.frames_per_buffer()) {
    sink_bus_->Zero();
    source_provider_->provideInput(audio_data,
                                   sink_params_.frames_per_buffer());
    EXPECT_DOUBLE_EQ(0.0, sink_bus_->channel(0)[0]);
    EXPECT_DOUBLE_EQ(0.0, sink_bus_->channel(1)[0]);
  }

  // Prepare the second packet for featching.
  source_provider_->DeliverData(source_bus_.get(), 0, 0, false);

  // Verify the packets.
  for (int i = 0; i < source_params_.frames_per_buffer();
       i += sink_params_.frames_per_buffer()) {
    sink_bus_->Zero();
    source_provider_->provideInput(audio_data,
                                   sink_params_.frames_per_buffer());
    EXPECT_GT(sink_bus_->channel(0)[0], 0);
    EXPECT_GT(sink_bus_->channel(1)[0], 0);
    EXPECT_DOUBLE_EQ(sink_bus_->channel(0)[0], sink_bus_->channel(1)[0]);
  }
}

TEST_F(WebRtcLocalAudioSourceProviderTest, VerifyAudioProcessingParams) {
  // Point the WebVector into memory owned by |sink_bus_|.
  blink::WebVector<float*> audio_data(
      static_cast<size_t>(sink_bus_->channels()));
  for (size_t i = 0; i < audio_data.size(); ++i)
    audio_data[i] = sink_bus_->channel(i);

  // Enable the source provider.
  source_provider_->provideInput(audio_data, sink_params_.frames_per_buffer());

  // Deliver data to |source_provider_| with audio processing params.
  int source_delay = 5;
  int source_volume = 255;
  bool source_key_pressed = true;
  source_provider_->DeliverData(source_bus_.get(), source_delay,
                                source_volume, source_key_pressed);

  int delay = 0, volume = 0;
  bool key_pressed = false;
  source_provider_->GetAudioProcessingParams(&delay, &volume, &key_pressed);
  EXPECT_EQ(volume, source_volume);
  EXPECT_EQ(key_pressed, source_key_pressed);
  int expected_delay = source_delay + static_cast<int>(
      source_bus_->frames() / source_params_.sample_rate() + 0.5);
  EXPECT_GE(delay, expected_delay);

  // Sleep a few ms to simulate processing time. This should increase the delay
  // value as time passes.
  int cached_delay = delay;
  const int kSleepMs = 10;
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(kSleepMs));
  source_provider_->GetAudioProcessingParams(&delay, &volume, &key_pressed);
  EXPECT_GT(delay, cached_delay);
}

}  // namespace content
