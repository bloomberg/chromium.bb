// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const int kBufferSize = 8192;
static const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;

class AudioRendererMixerManagerTest : public testing::Test {
 public:
  AudioRendererMixerManagerTest() {
    manager_.reset(new AudioRendererMixerManager(kSampleRate, kBufferSize));

    // We don't want to deal with instantiating a real AudioOutputDevice since
    // it's not important to our testing, so we inject a mock.
    mock_sink_ = new media::MockAudioRendererSink();
    manager_->SetAudioRendererSinkForTesting(mock_sink_);
  }

  media::AudioRendererMixer* GetMixer(const media::AudioParameters& params) {
    return manager_->GetMixer(params);
  }

  void RemoveMixer(const media::AudioParameters& params) {
    return manager_->RemoveMixer(params);
  }

  // Number of instantiated mixers.
  int mixer_count() {
    return manager_->mixers_.size();
  }

 protected:
  scoped_ptr<AudioRendererMixerManager> manager_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManagerTest);
};

// Verify GetMixer() and RemoveMixer() both work as expected; particularly with
// respect to the explicit ref counting done.
TEST_F(AudioRendererMixerManagerTest, GetRemoveMixer) {
  // Since we're testing two different sets of parameters, we expect
  // AudioRendererMixerManager to call Start and Stop on our mock twice.
  EXPECT_CALL(*mock_sink_, Start()).Times(2);
  EXPECT_CALL(*mock_sink_, Stop()).Times(2);

  // There should be no mixers outstanding to start with.
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params1(
      media::AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  media::AudioRendererMixer* mixer1 = GetMixer(params1);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(params1));
  EXPECT_EQ(mixer_count(), 1);

  // Remove the extra mixer we just acquired.
  RemoveMixer(params1);
  EXPECT_EQ(mixer_count(), 1);

  media::AudioParameters params2(
      media::AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate * 2,
      kBitsPerChannel, kBufferSize * 2);
  media::AudioRendererMixer* mixer2 = GetMixer(params2);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer_count(), 2);

  // Different parameters should result in a different mixer1.
  EXPECT_NE(mixer1, mixer2);

  // Remove both outstanding mixers.
  RemoveMixer(params1);
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(params2);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify CreateInput() provides AudioRendererMixerInput with the appropriate
// callbacks and they are working as expected.
TEST_F(AudioRendererMixerManagerTest, CreateInput) {
  // Since we're testing only one set of parameters, we expect
  // AudioRendererMixerManager to call Start and Stop on our mock once each.
  EXPECT_CALL(*mock_sink_, Start()).Times(1);
  EXPECT_CALL(*mock_sink_, Stop()).Times(1);

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  // Create a mixer input and ensure it doesn't instantiate a mixer yet.
  EXPECT_EQ(mixer_count(), 0);
  scoped_refptr<media::AudioRendererMixerInput> input(manager_->CreateInput());
  EXPECT_EQ(mixer_count(), 0);

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and remove it.
  media::FakeAudioRenderCallback callback(0);
  input->Initialize(params, &callback);
  EXPECT_EQ(mixer_count(), 1);

  // Destroying the input should destroy the mixer.
  input = NULL;
  EXPECT_EQ(mixer_count(), 0);
}

}  // namespace content
