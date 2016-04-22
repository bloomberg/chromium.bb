// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_mixer_manager.h"

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/audio_device_factory.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const int kBufferSize = 8192;
static const media::ChannelLayout kChannelLayout = media::CHANNEL_LAYOUT_STEREO;
static const media::ChannelLayout kAnotherChannelLayout =
    media::CHANNEL_LAYOUT_2_1;
static const char* const kDefaultDeviceId =
    media::AudioManagerBase::kDefaultDeviceId;
static const char kAnotherDeviceId[] = "another-device-id";
static const char kMatchedDeviceId[] = "matched-device-id";
static const char kNonexistentDeviceId[] = "nonexistent-device-id";

static const int kRenderFrameId = 124;
static const int kAnotherRenderFrameId = 678;

using media::AudioParameters;

class AudioRendererMixerManagerTest : public testing::Test,
                                      public AudioDeviceFactory {
 public:
  AudioRendererMixerManagerTest()
      : manager_(new AudioRendererMixerManager()),
        mock_sink_(new media::MockAudioRendererSink()),
        mock_sink_no_device_(new media::MockAudioRendererSink(
            kNonexistentDeviceId,
            media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND)),
        mock_sink_matched_device_(
            new media::MockAudioRendererSink(kMatchedDeviceId,
                                             media::OUTPUT_DEVICE_STATUS_OK)),
        mock_sink_for_session_id_(
            new media::MockAudioRendererSink(kMatchedDeviceId,
                                             media::OUTPUT_DEVICE_STATUS_OK)),
        kSecurityOrigin2(GURL("http://localhost")) {}

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

  // Number of instantiated mixers.
  int mixer_count() {
    return manager_->mixers_.size();
  }

 protected:
  MOCK_METHOD1(CreateAudioCapturerSource,
               scoped_refptr<media::AudioCapturerSource>(int));
  MOCK_METHOD5(
      CreateSwitchableAudioRendererSink,
      scoped_refptr<media::SwitchableAudioRendererSink>(SourceType,
                                                        int,
                                                        int,
                                                        const std::string&,
                                                        const url::Origin&));
  MOCK_METHOD5(CreateAudioRendererSink,
               scoped_refptr<media::AudioRendererSink>(SourceType,
                                                       int,
                                                       int,
                                                       const std::string&,
                                                       const url::Origin&));

  scoped_refptr<media::AudioRendererSink> CreateFinalAudioRendererSink(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin) {
    if ((device_id == kDefaultDeviceId) || (device_id == kAnotherDeviceId)) {
      // We don't care about separate sinks for these devices.
      return mock_sink_;
    }
    if (device_id == kNonexistentDeviceId)
      return mock_sink_no_device_;
    if (device_id.empty()) {
      // The sink used to get device ID from session ID if it's not empty
      return session_id ? mock_sink_for_session_id_ : mock_sink_;
    }
    if (device_id == kMatchedDeviceId)
      return mock_sink_matched_device_;

    NOTREACHED();
    return nullptr;
  }

  std::unique_ptr<AudioRendererMixerManager> manager_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_no_device_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_matched_device_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_for_session_id_;

  // To avoid global/static non-POD constants.
  const url::Origin kSecurityOrigin;
  const url::Origin kSecurityOrigin2;

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
  EXPECT_EQ(0, mixer_count());

  media::AudioParameters params1(
      AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
      kBitsPerChannel, kBufferSize);

  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1, mixer_count());

  // The same parameters should return the same mixer1.
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params1, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(1, mixer_count());

  // Remove the extra mixer we just acquired.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());

  media::AudioParameters params2(
      AudioParameters::AUDIO_PCM_LINEAR, kAnotherChannelLayout, kSampleRate * 2,
      kBitsPerChannel, kBufferSize * 2);
  media::AudioRendererMixer* mixer2 = GetMixer(
      kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(2, mixer_count());

  // Different parameters should result in a different mixer1.
  EXPECT_NE(mixer1, mixer2);

  // Remove both outstanding mixers.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(0, mixer_count());
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
  EXPECT_EQ(1, mixer_count());

  // Different sample rates, formats, bit depths, and buffer sizes should not
  // result in a different mixer.
  media::AudioParameters params2(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 kChannelLayout,
                                 kSampleRate * 2,
                                 kBitsPerChannel * 2,
                                 kBufferSize * 2);
  EXPECT_EQ(mixer1, GetMixer(kRenderFrameId, params2, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params2, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());

  // Modify some parameters that do matter: channel layout
  media::AudioParameters params3(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 kAnotherChannelLayout,
                                 kSampleRate,
                                 kBitsPerChannel,
                                 kBufferSize);
  ASSERT_NE(params3.channel_layout(), params1.channel_layout());

  EXPECT_NE(mixer1, GetMixer(kRenderFrameId, params3, kDefaultDeviceId,
                             kSecurityOrigin, nullptr));
  EXPECT_EQ(2, mixer_count());
  RemoveMixer(kRenderFrameId, params3, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());

  // Remove final mixer.
  RemoveMixer(kRenderFrameId, params1, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(0, mixer_count());
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
  EXPECT_EQ(0, mixer_count());
  media::FakeAudioRenderCallback callback(0);
  scoped_refptr<media::AudioRendererMixerInput> input(manager_->CreateInput(
      kRenderFrameId, 0, kDefaultDeviceId, kSecurityOrigin));
  input->Initialize(params, &callback);
  EXPECT_EQ(0, mixer_count());
  media::FakeAudioRenderCallback another_callback(1);
  scoped_refptr<media::AudioRendererMixerInput> another_input(
      manager_->CreateInput(kAnotherRenderFrameId, 0, kDefaultDeviceId,
                            kSecurityOrigin));
  another_input->Initialize(params, &another_callback);
  EXPECT_EQ(0, mixer_count());

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and remove it.
  input->Start();
  EXPECT_EQ(1, mixer_count());
  another_input->Start();
  EXPECT_EQ(2, mixer_count());

  // Destroying the inputs should destroy the mixers.
  input->Stop();
  input = nullptr;
  EXPECT_EQ(1, mixer_count());
  another_input->Stop();
  another_input = nullptr;
  EXPECT_EQ(0, mixer_count());
}

// Verify CreateInput() provided with session id creates AudioRendererMixerInput
// with the appropriate callbacks and they are working as expected.
TEST_F(AudioRendererMixerManagerTest, CreateInputWithSessionId) {
  // Expect AudioRendererMixerManager to call Start and Stop on our mock twice
  // each: for kDefaultDeviceId and for kAnotherDeviceId. Note: Under normal
  // conditions, each mixer would get its own sink!
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(2);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(2);

  // Expect AudioRendererMixerManager to call Start and Stop on the matched sink
  // once.
  EXPECT_CALL(*mock_sink_matched_device_.get(), Start()).Times(1);
  EXPECT_CALL(*mock_sink_matched_device_.get(), Stop()).Times(1);

  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  media::FakeAudioRenderCallback callback(0);
  EXPECT_EQ(0, mixer_count());

  // Empty device id, zero session id;
  scoped_refptr<media::AudioRendererMixerInput> input_to_default_device(
      manager_->CreateInput(kRenderFrameId, 0,  // session_id
                            std::string(), kSecurityOrigin));
  input_to_default_device->Initialize(params, &callback);
  EXPECT_EQ(0, mixer_count());

  // Specific device id, zero session id;
  scoped_refptr<media::AudioRendererMixerInput> input_to_matched_device(
      manager_->CreateInput(kRenderFrameId, 0,  // session_id
                            kMatchedDeviceId, kSecurityOrigin));
  input_to_matched_device->Initialize(params, &callback);
  EXPECT_EQ(0, mixer_count());

  // Specific device id, non-zero session id (to be ignored);
  scoped_refptr<media::AudioRendererMixerInput> input_to_another_device(
      manager_->CreateInput(kRenderFrameId, 1,  // session id
                            kAnotherDeviceId, kSecurityOrigin));
  input_to_another_device->Initialize(params, &callback);
  EXPECT_EQ(0, mixer_count());

  // Empty device id, non-zero session id;
  scoped_refptr<media::AudioRendererMixerInput>
      input_to_matched_device_with_session_id(
          manager_->CreateInput(kRenderFrameId, 2,  // session id
                                std::string(), kSecurityOrigin));
  input_to_matched_device_with_session_id->Initialize(params, &callback);
  EXPECT_EQ(0, mixer_count());

  // Implicitly test that AudioRendererMixerInput was provided with the expected
  // callbacks needed to acquire an AudioRendererMixer and remove it.
  input_to_default_device->Start();
  EXPECT_EQ(1, mixer_count());

  input_to_another_device->Start();
  EXPECT_EQ(2, mixer_count());

  input_to_matched_device->Start();
  EXPECT_EQ(3, mixer_count());

  // Should go to the same device as the input above.
  input_to_matched_device_with_session_id->Start();
  EXPECT_EQ(3, mixer_count());

  // Destroying the inputs should destroy the mixers.
  input_to_default_device->Stop();
  input_to_default_device = nullptr;
  EXPECT_EQ(2, mixer_count());
  input_to_another_device->Stop();
  input_to_another_device = nullptr;
  EXPECT_EQ(1, mixer_count());
  input_to_matched_device->Stop();
  input_to_matched_device = nullptr;
  EXPECT_EQ(1, mixer_count());
  input_to_matched_device_with_session_id->Stop();
  input_to_matched_device_with_session_id = nullptr;
  EXPECT_EQ(0, mixer_count());
}

// Verify GetMixer() correctly creates different mixers with the same
// parameters, but different device ID and/or security origin
TEST_F(AudioRendererMixerManagerTest, MixerDevices) {
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(3);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(3);
  EXPECT_EQ(0, mixer_count());

  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1, mixer_count());

  media::AudioRendererMixer* mixer2 = GetMixer(
      kRenderFrameId, params, kAnotherDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(2, mixer_count());
  EXPECT_NE(mixer1, mixer2);

  media::AudioRendererMixer* mixer3 = GetMixer(
      kRenderFrameId, params, kAnotherDeviceId, kSecurityOrigin2, nullptr);
  ASSERT_TRUE(mixer3);
  EXPECT_EQ(3, mixer_count());
  EXPECT_NE(mixer1, mixer3);
  EXPECT_NE(mixer2, mixer3);

  RemoveMixer(kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(2, mixer_count());
  RemoveMixer(kRenderFrameId, params, kAnotherDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params, kAnotherDeviceId, kSecurityOrigin2);
  EXPECT_EQ(0, mixer_count());
}

// Verify GetMixer() correctly deduplicate mixers with the same
// parameters, different security origins but default device ID
TEST_F(AudioRendererMixerManagerTest, OneMixerDifferentOriginsDefaultDevice) {
  EXPECT_CALL(*mock_sink_.get(), Start()).Times(1);
  EXPECT_CALL(*mock_sink_.get(), Stop()).Times(1);
  EXPECT_EQ(0, mixer_count());

  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  media::AudioRendererMixer* mixer1 = GetMixer(
      kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer1);
  EXPECT_EQ(1, mixer_count());

  media::AudioRendererMixer* mixer2 =
      GetMixer(kRenderFrameId, params, std::string(), kSecurityOrigin, nullptr);
  ASSERT_TRUE(mixer2);
  EXPECT_EQ(1, mixer_count());
  EXPECT_EQ(mixer1, mixer2);

  media::AudioRendererMixer* mixer3 = GetMixer(
      kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin2, nullptr);
  ASSERT_TRUE(mixer3);
  EXPECT_EQ(1, mixer_count());
  EXPECT_EQ(mixer1, mixer3);

  media::AudioRendererMixer* mixer4 = GetMixer(
      kRenderFrameId, params, std::string(), kSecurityOrigin2, nullptr);
  ASSERT_TRUE(mixer4);
  EXPECT_EQ(1, mixer_count());
  EXPECT_EQ(mixer1, mixer4);

  RemoveMixer(kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params, std::string(), kSecurityOrigin);
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params, kDefaultDeviceId, kSecurityOrigin2);
  EXPECT_EQ(1, mixer_count());
  RemoveMixer(kRenderFrameId, params, std::string(), kSecurityOrigin2);
  EXPECT_EQ(0, mixer_count());
}

// Verify that GetMixer() correctly returns a null mixer and an appropriate
// status code when a nonexistent device is requested.
TEST_F(AudioRendererMixerManagerTest, NonexistentDevice) {
  EXPECT_EQ(0, mixer_count());
  media::AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR,
                                kChannelLayout, kSampleRate, kBitsPerChannel,
                                kBufferSize);
  media::OutputDeviceStatus device_status = media::OUTPUT_DEVICE_STATUS_OK;
  EXPECT_CALL(*mock_sink_no_device_.get(), Stop());
  media::AudioRendererMixer* mixer =
      GetMixer(kRenderFrameId, params, kNonexistentDeviceId, kSecurityOrigin,
               &device_status);
  EXPECT_FALSE(mixer);
  EXPECT_EQ(media::OUTPUT_DEVICE_STATUS_ERROR_NOT_FOUND, device_status);
  EXPECT_EQ(0, mixer_count());
}

}  // namespace content
