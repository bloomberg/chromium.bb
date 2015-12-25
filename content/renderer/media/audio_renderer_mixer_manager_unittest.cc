// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/audio_renderer_mixer_manager.h"
#include "ipc/ipc_message.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_hardware_config.h"
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
static const media::ChannelLayout kAnotherChannelLayout =
    media::CHANNEL_LAYOUT_2_1;
static const std::string kDefaultDeviceId;
static const url::Origin kSecurityOrigin;

static const int kRenderFrameId = 124;
static const int kAnotherRenderFrameId = 678;

using media::AudioParameters;

class AudioRendererMixerManagerTest : public testing::Test {
 public:
  AudioRendererMixerManagerTest() {
    manager_.reset(new AudioRendererMixerManager());

    // We don't want to deal with instantiating a real AudioOutputDevice since
    // it's not important to our testing, so we inject a mock.
    mock_sink_ = new media::MockAudioRendererSink();
    manager_->SetAudioRendererSinkForTesting(mock_sink_.get());
  }

  media::AudioRendererMixer* GetMixer(
      int source_render_frame_id,
      const media::AudioParameters& params,
      const std::string& device_id,
      const url::Origin& security_origin,
      media::OutputDeviceStatus* device_status) {
    return manager_->GetMixer(source_render_frame_id, params, device_id,
                              security_origin, device_status);
  }

  void RemoveMixer(int source_render_frame_id,
                   const media::AudioParameters& params,
                   const std::string& device_id,
                   const url::Origin& security_origin) {
    return manager_->RemoveMixer(source_render_frame_id, params, device_id,
                                 security_origin);
  }

  void UseNonexistentSink() {
    mock_sink_ = new media::MockAudioRendererSink(
        media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
    manager_->SetAudioRendererSinkForTesting(mock_sink_.get());
  }

  // Number of instantiated mixers.
  int mixer_count() {
    return manager_->mixers_.size();
  }

 protected:
  scoped_ptr<AudioRendererMixerManager> manager_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerManagerTest);
};

// Verify GetMixer() and RemoveMixer() both work as expected; particularly with
// respect to the explicit ref counting done.
TEST_F(AudioRendererMixerManagerTest, GetRemoveMixer) {
  // Since we're testing two different sets of parameters, we expect
  // AudioRendererMixerManager to call Start and Stop on our mock twice.
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);

  // There should be no mixers outstanding to start with.
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params1(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params1, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(mixer_count(), 1);

  // Remove the extra mixer we just acquired.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 1);

  media::AudioParameters params2(
      AudioParameters::AUDIO_PCM_LINEAR, kAnotherChannelLayout, kSampleRate * 2,
      kBitsPerChannel, kBufferSize * 2);
  media::AudioRendererMixer* mixer2 = GetMixer(
      kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer_count(), 2);

  // Different parameters should result in a different mixer1.
  EXPECT_NE(mixer1, mixer2);

  // Remove both outstanding mixers.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify GetMixer() correctly deduplicates mixer with irrelevant AudioParameter
// differences.
TEST_F(AudioRendererMixerManagerTest, MixerReuse) {
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params1(AudioParameters::AUDIO_PCM_LINEAR,
                                 kChannelLayout,
                                 kSampleRate,
                                 kBitsPerChannel,
                                 kBufferSize);
  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  // Different sample rates, formats, bit depths, and buffer sizes should not
  // result in a different mixer.
  media::AudioParameters params2(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 kChannelLayout,
                                 kSampleRate * 2,
                                 kBitsPerChannel * 2,
                                 kBufferSize * 2);
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params2, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 1);

  // Modify some parameters that do matter: channel layout
  media::AudioParameters params3(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 kAnotherChannelLayout,
                                 kSampleRate,
                                 kBitsPerChannel,
                                 kBufferSize);
  ASSERT_NE(params3.channel_layout(), params1.channel_layout());

  EXPECT_NE(mixer1, GetMixer(kRenderFrameId, params3, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(mixer_count(), 2);
  RemoveMixer(kRenderFrameId, params3, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 1);

  // Remove final mixer.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify CreateInput() provides AudioRendererMixerInput with the appropriate
// callbacks and they are working as expected.  Also, verify that separate
// mixers are created for separate render views, even though the AudioParameters
// are the same.
TEST_F(AudioRendererMixerManagerTest, CreateInput) {
  // Expect AudioRendererMixerManager to call Start and Stop on our mock twice
  // each.  Note: Under normal conditions, each mixer would get its own sink!
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);

  media::AudioParameters params(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  // Create two mixer inputs and ensure this doesn't instantiate any mixers yet.
  EXPECT_EQ(mixer_count(), 0);
  media::FakeAudioRenderCallback callback(0);
  scoped_refptr<media::AudioRendererMixerInput> input(
      manager_->CreateInput(kRenderFrameId, kDefaultDeviceId, kSecurityOrigin));
  input->Initialize(params, &callback);
  EXPECT_EQ(mixer_count(), 0);
  media::FakeAudioRenderCallback another_callback(1);
  scoped_refptr<media::AudioRendererMixerInput> another_input(
      manager_->CreateInput(kAnotherRenderFrameId, kDefaultDeviceId,
                            kSecurityOrigin));
  another_input->Initialize(params, &another_callback);
  EXPECT_EQ(mixer_count(), 0);

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and remove it.
  input->Start();
  EXPECT_EQ(mixer_count(), 1);
  another_input->Start();
  EXPECT_EQ(mixer_count(), 2);

  // Destroying the inputs should destroy the mixers.
  input->Stop();
  input = NULL;
  EXPECT_EQ(mixer_count(), 1);
  another_input->Stop();
  another_input = NULL;
  EXPECT_EQ(mixer_count(), 0);
}

// Verify GetMixer() correctly creates different mixers with the same
// parameters, but different device ID and/or security origin
TEST_F(AudioRendererMixerManagerTest, MixerDevices) {
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(3);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(3);
  EXPECT_EQ(mixer_count(), 0);

  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(mixer_count(), 1);

  std::string device_id2("fake-device-id");
  media::AudioRendererMixer* mixer2 =
      GetMixer(kRenderFrameId, params, device_id2, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(mixer_count(), 2);
  EXPECT_NE(mixer1, mixer2);

  url::Origin security_origin2(GURL("http://localhost"));
  media::AudioRendererMixer* mixer3 = GetMixer(
      kRenderFrameId, params, kDefaultDeviceId, security_origin2, nullptr);
  ASSERT_TRUE(mixer3);
  EXPECT_EQ(mixer_count(), 3);
  EXPECT_NE(mixer1, mixer3);
  EXPECT_NE(mixer2, mixer3);

  RemoveMixer(kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 2);
  RemoveMixer(kRenderFrameId, params, device_id2, kSecurityOrigin);
  EXPECT_EQ(mixer_count(), 1);
  RemoveMixer(kRenderFrameId, params, kDefaultDeviceId, security_origin2);
  EXPECT_EQ(mixer_count(), 0);
}

// Verify that GetMixer() correctly returns a null mixer and an appropriate
// status code when a nonexistent device is requested.
TEST_F(AudioRendererMixerManagerTest, NonexistentDevice) {
  EXPECT_EQ(mixer_count(), 0);
  UseNonexistentSink();
  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  std::string nonexistent_device_id("nonexistent-device-id");
  media::OutputDeviceStatus device_status = media::OUTPUT_DEVICE_STATUS_OK;
  EXPECT_CALL(*mock_sink_.get(), Stop());
  media::AudioRendererMixer* mixer =
      GetMixer(kRenderFrameId, params, nonexistent_device_id, kSecurityOrigin,
               &device_status);
  EXPECT_FALSE(mixer);
  EXPECT_EQ(device_status, media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND);
  EXPECT_EQ(mixer_count(), 0);
}

}  // namespace content
