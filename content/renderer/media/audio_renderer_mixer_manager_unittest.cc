// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/audio_device_factory.h"
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
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;

// By sub-classing AudioDeviceFactory we've overridden the factory to use our
// CreateAudioDevice() method globally.
class MockAudioRenderSinkFactory : public AudioDeviceFactory {
 public:
  MockAudioRenderSinkFactory() {}
  virtual ~MockAudioRenderSinkFactory() {}

 protected:
  virtual media::MockAudioRendererSink* CreateAudioDevice() {
    media::MockAudioRendererSink* sink = new media::MockAudioRendererSink();
    EXPECT_CALL(*sink, Start());
    EXPECT_CALL(*sink, Stop());
    return sink;
  }

  DISALLOW_COPY_AND_ASSIGN(MockAudioRenderSinkFactory);
};

class AudioRendererMixerManagerTest : public testing::Test {
 public:
  AudioRendererMixerManagerTest() {
    // We don't want to deal with instantiating a real AudioDevice since it's
    // not important to our testing, so use a mock AudioDeviceFactory.
    mock_sink_factory_.reset(new MockAudioRenderSinkFactory());
    manager_.reset(new AudioRendererMixerManager(kSampleRate, kBufferSize));
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
  scoped_ptr<MockAudioRenderSinkFactory> mock_sink_factory_;
  scoped_ptr<AudioRendererMixerManager> manager_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManagerTest);
};

// Verify GetMixer() and RemoveMixer() both work as expected; particularly with
// respect to the explicit ref counting done.
TEST_F(AudioRendererMixerManagerTest, GetRemoveMixer) {
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
