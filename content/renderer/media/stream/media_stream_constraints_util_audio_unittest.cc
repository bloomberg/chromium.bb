// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/renderer/media/stream/local_media_stream_audio_source.h"
#include "content/renderer/media/stream/mock_constraint_factory.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "media/base/audio_parameters.h"
#include "media/webrtc/webrtc_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_source.h"
#include "third_party/blink/public/platform/modules/mediastream/web_platform_media_stream_source.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

using blink::AudioCaptureSettings;
using blink::AudioProcessingProperties;
using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;

namespace {

using BoolSetFunction = void (blink::BooleanConstraint::*)(bool);
using StringSetFunction =
    void (blink::StringConstraint::*)(const blink::WebString&);
using MockFactoryAccessor =
    blink::WebMediaTrackConstraintSet& (MockConstraintFactory::*)();

const BoolSetFunction kBoolSetFunctions[] = {
    &blink::BooleanConstraint::SetExact, &blink::BooleanConstraint::SetIdeal,
};

const StringSetFunction kStringSetFunctions[] = {
    &blink::StringConstraint::SetExact, &blink::StringConstraint::SetIdeal,
};

const MockFactoryAccessor kFactoryAccessors[] = {
    &MockConstraintFactory::basic, &MockConstraintFactory::AddAdvanced};

const bool kBoolValues[] = {true, false};

const int kMinChannels = 1;

using AudioSettingsBoolMembers =
    std::vector<bool (AudioCaptureSettings::*)() const>;
using AudioPropertiesBoolMembers =
    std::vector<bool AudioProcessingProperties::*>;

template <typename T>
static bool Contains(const std::vector<T>& vector, T value) {
  return base::ContainsValue(vector, value);
}

}  // namespace

class MediaStreamConstraintsUtilAudioTestBase {
 protected:
  void MakeSystemEchoCancellerDeviceExperimental() {
    media::AudioParameters experimental_system_echo_canceller_parameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 1000);
    experimental_system_echo_canceller_parameters.set_effects(
        media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER);
    capabilities_[1] = {"experimental_system_echo_canceller_device",
                        "fake_group3",
                        experimental_system_echo_canceller_parameters};
  }

  void SetMediaStreamSource(const std::string& source) {}

  void ResetFactory() {
    constraint_factory_.Reset();
    constraint_factory_.basic().media_stream_source.SetExact(
        blink::WebString::FromASCII(GetMediaStreamSource()));
  }

  // If not overridden, this function will return device capture by default.
  virtual std::string GetMediaStreamSource() { return std::string(); }
  bool IsDeviceCapture() { return GetMediaStreamSource().empty(); }

  blink::MediaStreamType GetMediaStreamType() {
    std::string media_source = GetMediaStreamSource();
    if (media_source.empty())
      return blink::MEDIA_DEVICE_AUDIO_CAPTURE;
    else if (media_source == blink::kMediaStreamSourceTab)
      return blink::MEDIA_GUM_TAB_AUDIO_CAPTURE;
    return blink::MEDIA_GUM_DESKTOP_AUDIO_CAPTURE;
  }

  std::unique_ptr<ProcessedLocalAudioSource> GetProcessedLocalAudioSource(
      const AudioProcessingProperties& properties,
      bool disable_local_echo,
      bool render_to_associated_sink,
      int effects) {
    blink::MediaStreamDevice device;
    device.id = "processed_source";
    device.type = GetMediaStreamType();
    if (render_to_associated_sink)
      device.matched_output_device_id = std::string("some_device_id");
    device.input.set_effects(effects);

    return std::make_unique<ProcessedLocalAudioSource>(
        -1, device, disable_local_echo, properties,
        blink::WebPlatformMediaStreamSource::ConstraintsCallback(),
        &pc_factory_, blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  std::unique_ptr<ProcessedLocalAudioSource> GetProcessedLocalAudioSource(
      const AudioProcessingProperties& properties,
      bool disable_local_echo,
      bool render_to_associated_sink) {
    return GetProcessedLocalAudioSource(
        properties, disable_local_echo, render_to_associated_sink,
        media::AudioParameters::PlatformEffectsMask::NO_EFFECTS);
  }

  std::unique_ptr<LocalMediaStreamAudioSource> GetLocalMediaStreamAudioSource(
      bool enable_system_echo_canceller,
      bool disable_local_echo,
      bool render_to_associated_sink,
      bool enable_experimental_echo_canceller = false,
      const int* requested_buffer_size = nullptr) {
    blink::MediaStreamDevice device;
    device.type = GetMediaStreamType();

    int effects = 0;
    if (enable_system_echo_canceller)
      effects |= media::AudioParameters::ECHO_CANCELLER;
    if (enable_experimental_echo_canceller)
      effects |= media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;
    device.input.set_effects(effects);

    if (render_to_associated_sink)
      device.matched_output_device_id = std::string("some_device_id");

    return std::make_unique<LocalMediaStreamAudioSource>(
        -1, device, requested_buffer_size, disable_local_echo,
        blink::WebPlatformMediaStreamSource::ConstraintsCallback(),
        blink::scheduler::GetSingleThreadTaskRunnerForTesting());
  }

  AudioCaptureSettings SelectSettings() {
    blink::WebMediaConstraints constraints =
        constraint_factory_.CreateWebMediaConstraints();
    return SelectSettingsAudioCapture(capabilities_, constraints, false);
  }

  // When googExperimentalEchoCancellation is not explicitly set, its default
  // value is always false on Android. On other platforms it behaves like other
  // audio-processing properties.
  void CheckGoogExperimentalEchoCancellationDefault(
      const AudioProcessingProperties& properties,
      bool value) {
#if defined(OS_ANDROID)
    EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
#else
    EXPECT_EQ(value, properties.goog_experimental_echo_cancellation);
#endif
  }

  void CheckBoolDefaultsDeviceCapture(
      const AudioSettingsBoolMembers& exclude_main_settings,
      const AudioPropertiesBoolMembers& exclude_audio_properties,
      const AudioCaptureSettings& result) {
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::disable_local_echo)) {
      EXPECT_TRUE(result.disable_local_echo());
    }
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::render_to_associated_sink)) {
      EXPECT_FALSE(result.render_to_associated_sink());
    }

    const auto& properties = result.audio_processing_properties();
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_audio_mirroring)) {
      EXPECT_FALSE(properties.goog_audio_mirroring);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_auto_gain_control)) {
      EXPECT_TRUE(properties.goog_auto_gain_control);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_echo_cancellation)) {
      CheckGoogExperimentalEchoCancellationDefault(properties, true);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_typing_noise_detection)) {
      EXPECT_TRUE(properties.goog_typing_noise_detection);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_noise_suppression)) {
      EXPECT_TRUE(properties.goog_noise_suppression);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_noise_suppression)) {
      EXPECT_TRUE(properties.goog_experimental_noise_suppression);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_highpass_filter)) {
      EXPECT_TRUE(properties.goog_highpass_filter);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_auto_gain_control)) {
      EXPECT_TRUE(properties.goog_experimental_auto_gain_control);
    }
  }

  void CheckBoolDefaultsContentCapture(
      const AudioSettingsBoolMembers& exclude_main_settings,
      const AudioPropertiesBoolMembers& exclude_audio_properties,
      const AudioCaptureSettings& result) {
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::disable_local_echo)) {
      EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
                result.disable_local_echo());
    }
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::render_to_associated_sink)) {
      EXPECT_FALSE(result.render_to_associated_sink());
    }

    const auto& properties = result.audio_processing_properties();
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_audio_mirroring)) {
      EXPECT_FALSE(properties.goog_audio_mirroring);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_auto_gain_control)) {
      EXPECT_FALSE(properties.goog_auto_gain_control);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_echo_cancellation)) {
      EXPECT_FALSE(properties.goog_experimental_echo_cancellation);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_typing_noise_detection)) {
      EXPECT_FALSE(properties.goog_typing_noise_detection);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_noise_suppression)) {
      EXPECT_FALSE(properties.goog_noise_suppression);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_noise_suppression)) {
      EXPECT_FALSE(properties.goog_experimental_noise_suppression);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::goog_highpass_filter)) {
      EXPECT_FALSE(properties.goog_highpass_filter);
    }
    if (!Contains(
            exclude_audio_properties,
            &AudioProcessingProperties::goog_experimental_auto_gain_control)) {
      EXPECT_FALSE(properties.goog_experimental_auto_gain_control);
    }
  }

  void CheckBoolDefaults(
      const AudioSettingsBoolMembers& exclude_main_settings,
      const AudioPropertiesBoolMembers& exclude_audio_properties,
      const AudioCaptureSettings& result) {
    if (IsDeviceCapture()) {
      CheckBoolDefaultsDeviceCapture(exclude_main_settings,
                                     exclude_audio_properties, result);
    } else {
      CheckBoolDefaultsContentCapture(exclude_main_settings,
                                      exclude_audio_properties, result);
    }
  }

  void CheckEchoCancellationTypeDefault(const AudioCaptureSettings& result) {
    const auto& properties = result.audio_processing_properties();
    if (IsDeviceCapture()) {
      EXPECT_EQ(properties.echo_cancellation_type,
                EchoCancellationType::kEchoCancellationAec3);
    } else {
      EXPECT_EQ(properties.echo_cancellation_type,
                EchoCancellationType::kEchoCancellationDisabled);
    }
  }

  void CheckDevice(const AudioDeviceCaptureCapability& expected_device,
                   const AudioCaptureSettings& result) {
    EXPECT_EQ(expected_device.DeviceID(), result.device_id());
  }

  void CheckDeviceDefaults(const AudioCaptureSettings& result) {
    if (IsDeviceCapture())
      CheckDevice(*default_device_, result);
    else
      EXPECT_TRUE(result.device_id().empty());
  }

  void CheckAllDefaults(
      const AudioSettingsBoolMembers& exclude_main_settings,
      const AudioPropertiesBoolMembers& exclude_audio_properties,
      const AudioCaptureSettings& result) {
    CheckBoolDefaults(exclude_main_settings, exclude_audio_properties, result);
    CheckEchoCancellationTypeDefault(result);
    CheckDeviceDefaults(result);
  }

  // Assumes that echoCancellation is set to true as a basic, exact constraint.
  void CheckAudioProcessingPropertiesForExactEchoCancellationType(
      const blink::WebString& echo_cancellation_type_constraint,
      const AudioCaptureSettings& result) {
    const AudioProcessingProperties& properties =
        result.audio_processing_properties();

    // With device capture, the echo_cancellation constraint
    // enables/disables all audio processing by default.
    // With content capture, the echo_cancellation constraint controls
    // only the echo_cancellation properties. The other audio processing
    // properties default to false.
    const EchoCancellationType expected_ec_type =
        GetEchoCancellationTypeFromConstraintString(
            echo_cancellation_type_constraint);
    if (!IsDeviceCapture()) {
      ASSERT_NE(EchoCancellationType::kEchoCancellationSystem,
                expected_ec_type);
    }
    EXPECT_EQ(expected_ec_type, properties.echo_cancellation_type);

    const bool enable_webrtc_audio_processing = IsDeviceCapture();
    EXPECT_EQ(enable_webrtc_audio_processing,
              properties.goog_auto_gain_control);
    CheckGoogExperimentalEchoCancellationDefault(
        properties, enable_webrtc_audio_processing);
    EXPECT_EQ(enable_webrtc_audio_processing,
              properties.goog_typing_noise_detection);
    EXPECT_EQ(enable_webrtc_audio_processing,
              properties.goog_noise_suppression);
    EXPECT_EQ(enable_webrtc_audio_processing,
              properties.goog_experimental_noise_suppression);
    EXPECT_EQ(enable_webrtc_audio_processing, properties.goog_highpass_filter);
    EXPECT_EQ(enable_webrtc_audio_processing,
              properties.goog_experimental_auto_gain_control);

    // The following are not audio processing.
    EXPECT_FALSE(properties.goog_audio_mirroring);
    EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
              result.disable_local_echo());
    EXPECT_FALSE(result.render_to_associated_sink());
    if (IsDeviceCapture()) {
      CheckDevice(
          expected_ec_type == EchoCancellationType::kEchoCancellationSystem
              ? *system_echo_canceller_device_
              : *default_device_,
          result);
    } else {
      EXPECT_TRUE(result.device_id().empty());
    }
  }

  void CheckAudioProcessingPropertiesForIdealEchoCancellationType(
      const AudioCaptureSettings& result) {
    const AudioProcessingProperties& properties =
        result.audio_processing_properties();

    EXPECT_EQ(EchoCancellationType::kEchoCancellationSystem,
              properties.echo_cancellation_type);
    EXPECT_TRUE(properties.goog_auto_gain_control);
    CheckGoogExperimentalEchoCancellationDefault(properties, true);
    EXPECT_TRUE(properties.goog_typing_noise_detection);
    EXPECT_TRUE(properties.goog_noise_suppression);
    EXPECT_TRUE(properties.goog_experimental_noise_suppression);
    EXPECT_TRUE(properties.goog_highpass_filter);
    EXPECT_TRUE(properties.goog_experimental_auto_gain_control);

    // The following are not audio processing.
    EXPECT_FALSE(properties.goog_audio_mirroring);
    EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
              result.disable_local_echo());
    EXPECT_FALSE(result.render_to_associated_sink());
    CheckDevice(*system_echo_canceller_device_, result);
  }

  EchoCancellationType GetEchoCancellationTypeFromConstraintString(
      const blink::WebString& constraint_string) {
    if (constraint_string == kEchoCancellationTypeValues[0])
      return EchoCancellationType::kEchoCancellationAec3;
    if (constraint_string == kEchoCancellationTypeValues[1])
      return EchoCancellationType::kEchoCancellationAec3;
    if (constraint_string == kEchoCancellationTypeValues[2])
      return EchoCancellationType::kEchoCancellationSystem;

    ADD_FAILURE() << "Invalid echo cancellation type constraint: "
                  << constraint_string.Ascii();
    return EchoCancellationType::kEchoCancellationDisabled;
  }

  void CheckLatencyConstraint(const AudioDeviceCaptureCapability* device,
                              double min_latency,
                              double max_latency) {
    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device->DeviceID()));
    constraint_factory_.basic().echo_cancellation.SetExact(false);
    constraint_factory_.basic().latency.SetExact(0.0);
    auto result = SelectSettings();
    EXPECT_FALSE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device->DeviceID()));
    constraint_factory_.basic().echo_cancellation.SetExact(false);
    constraint_factory_.basic().latency.SetMin(max_latency + 0.001);
    result = SelectSettings();
    EXPECT_FALSE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device->DeviceID()));
    constraint_factory_.basic().echo_cancellation.SetExact(false);
    constraint_factory_.basic().latency.SetMax(min_latency - 0.001);
    result = SelectSettings();
    EXPECT_FALSE(result.HasValue());

    CheckLocalMediaStreamAudioSourceLatency(
        device, 0.001, min_latency * device->Parameters().sample_rate());
    CheckLocalMediaStreamAudioSourceLatency(
        device, 1.0, max_latency * device->Parameters().sample_rate());
  }

  void CheckLocalMediaStreamAudioSourceLatency(
      const AudioDeviceCaptureCapability* device,
      double requested_latency,
      int expected_buffer_size) {
    constraint_factory_.Reset();
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device->DeviceID()));
    constraint_factory_.basic().echo_cancellation.SetExact(false);
    constraint_factory_.basic().latency.SetIdeal(requested_latency);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());

    std::unique_ptr<LocalMediaStreamAudioSource> local_source =
        GetLocalMediaStreamAudioSource(
            false /* enable_system_echo_canceller */,
            false /* disable_local_echo */,
            false /* render_to_associated_sink */,
            false /* enable_experimental_echo_canceller */,
            base::OptionalOrNullptr(result.requested_buffer_size()));
    EXPECT_EQ(local_source->GetAudioParameters().frames_per_buffer(),
              expected_buffer_size);
  }

  MockConstraintFactory constraint_factory_;
  AudioDeviceCaptureCapabilities capabilities_;
  const AudioDeviceCaptureCapability* default_device_ = nullptr;
  const AudioDeviceCaptureCapability* system_echo_canceller_device_ = nullptr;
  const AudioDeviceCaptureCapability* four_channels_device_ = nullptr;
  const AudioDeviceCaptureCapability* variable_latency_device_ = nullptr;
  const std::vector<media::Point> kMicPositions = {{8, 8, 8}, {4, 4, 4}};

  // TODO(grunell): Store these as separate constants and compare against those
  // in tests, instead of indexing the vector.
  const std::vector<blink::WebString> kEchoCancellationTypeValues = {
      blink::WebString::FromASCII("browser"),
      blink::WebString::FromASCII("aec3"),
      blink::WebString::FromASCII("system")};

 private:
  // Required for tests involving a MediaStreamAudioSource.
  base::test::ScopedTaskEnvironment task_environment_;
  MockPeerConnectionDependencyFactory pc_factory_;
};

class MediaStreamConstraintsUtilAudioTest
    : public MediaStreamConstraintsUtilAudioTestBase,
      public testing::TestWithParam<std::string> {
 public:
  void SetUp() override {
    ResetFactory();
    if (IsDeviceCapture()) {
      capabilities_.emplace_back(
          "default_device", "fake_group1",
          media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::CHANNEL_LAYOUT_STEREO,
                                 media::AudioParameters::kAudioCDSampleRate,
                                 1000));

      media::AudioParameters system_echo_canceller_parameters(
          media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
          media::CHANNEL_LAYOUT_STEREO,
          media::AudioParameters::kAudioCDSampleRate, 1000);
      system_echo_canceller_parameters.set_effects(
          media::AudioParameters::ECHO_CANCELLER);
      capabilities_.emplace_back("system_echo_canceller_device", "fake_group2",
                                 system_echo_canceller_parameters);

      capabilities_.emplace_back(
          "4_channels_device", "fake_group3",
          media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::CHANNEL_LAYOUT_4_0,
                                 media::AudioParameters::kAudioCDSampleRate,
                                 1000));

      capabilities_.emplace_back(
          "8khz_sample_rate_device", "fake_group4",
          media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::CHANNEL_LAYOUT_STEREO,
                                 blink::AudioProcessing::kSampleRate8kHz,
                                 1000));

      capabilities_.emplace_back(
          "variable_latency_device", "fake_group5",
          media::AudioParameters(
              media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
              media::CHANNEL_LAYOUT_STEREO,
              media::AudioParameters::kAudioCDSampleRate, 512,
              media::AudioParameters::HardwareCapabilities(128, 4096)));

      default_device_ = &capabilities_[0];
      system_echo_canceller_device_ = &capabilities_[1];
      four_channels_device_ = &capabilities_[2];
      variable_latency_device_ = &capabilities_[4];
    } else {
      // For content capture, use a single capability that admits all possible
      // settings.
      capabilities_.emplace_back();
    }
  }

  std::string GetMediaStreamSource() override { return GetParam(); }
};

class MediaStreamConstraintsRemoteAPMTest
    : public MediaStreamConstraintsUtilAudioTestBase,
      public testing::TestWithParam<bool> {
  void SetUp() override {
    if (UseRemoteAPMFlag()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kWebRtcApmInAudioService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kWebRtcApmInAudioService);
    }

    // Setup the capabilities.
    ResetFactory();
    if (IsDeviceCapture()) {
      capabilities_.emplace_back(
          "default_device", "fake_group1",
          media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                 media::CHANNEL_LAYOUT_STEREO,
                                 media::AudioParameters::kAudioCDSampleRate,
                                 1000));
      default_device_ = &capabilities_[0];
    }
  }

  bool UseRemoteAPMFlag() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The Unconstrained test checks the default selection criteria.
TEST_P(MediaStreamConstraintsUtilAudioTest, Unconstrained) {
  auto result = SelectSettings();

  // All settings should have default values.
  EXPECT_TRUE(result.HasValue());
  CheckAllDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                   result);
}

// This test checks all possible ways to set boolean constraints (except
// echo cancellation constraints, which are not mapped 1:1 to output audio
// processing properties).
TEST_P(MediaStreamConstraintsUtilAudioTest, SingleBoolConstraint) {
  AudioSettingsBoolMembers kMainSettings = {
      &AudioCaptureSettings::disable_local_echo,
      &AudioCaptureSettings::render_to_associated_sink};

  const std::vector<
      blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
      kMainBoolConstraints = {
          &blink::WebMediaTrackConstraintSet::disable_local_echo,
          &blink::WebMediaTrackConstraintSet::render_to_associated_sink};

  ASSERT_EQ(kMainSettings.size(), kMainBoolConstraints.size());
  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (size_t i = 0; i < kMainSettings.size(); ++i) {
        for (bool value : kBoolValues) {
          ResetFactory();
          (((constraint_factory_.*accessor)().*kMainBoolConstraints[i]).*
           set_function)(value);
          auto result = SelectSettings();
          EXPECT_TRUE(result.HasValue());
          EXPECT_EQ(value, (result.*kMainSettings[i])());
          CheckAllDefaults({kMainSettings[i]}, AudioPropertiesBoolMembers(),
                           result);
        }
      }
    }
  }

  const AudioPropertiesBoolMembers kAudioProcessingProperties = {
      &AudioProcessingProperties::goog_audio_mirroring,
      &AudioProcessingProperties::goog_auto_gain_control,
      &AudioProcessingProperties::goog_experimental_echo_cancellation,
      &AudioProcessingProperties::goog_typing_noise_detection,
      &AudioProcessingProperties::goog_noise_suppression,
      &AudioProcessingProperties::goog_experimental_noise_suppression,
      &AudioProcessingProperties::goog_highpass_filter,
      &AudioProcessingProperties::goog_experimental_auto_gain_control};

  const std::vector<
      blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
      kAudioProcessingConstraints = {
          &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
          &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_echo_cancellation,
          &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
          &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_noise_suppression,
          &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_auto_gain_control,
      };

  ASSERT_EQ(kAudioProcessingProperties.size(),
            kAudioProcessingConstraints.size());
  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (size_t i = 0; i < kAudioProcessingProperties.size(); ++i) {
        for (bool value : kBoolValues) {
          ResetFactory();
          (((constraint_factory_.*accessor)().*kAudioProcessingConstraints[i]).*
           set_function)(value);
          auto result = SelectSettings();
          EXPECT_TRUE(result.HasValue());
          EXPECT_EQ(value, result.audio_processing_properties().*
                               kAudioProcessingProperties[i]);
          CheckAllDefaults(AudioSettingsBoolMembers(),
                           {kAudioProcessingProperties[i]}, result);
        }
      }
    }
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, SampleSize) {
  ResetFactory();
  constraint_factory_.basic().sample_size.SetExact(16);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetExact(0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Only set a min value for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_size.SetMin(16);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetMin(17);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Only set a max value for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_size.SetMax(16);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetMax(15);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Define a bounded range for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_size.SetMin(10);
  constraint_factory_.basic().sample_size.SetMax(20);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetMin(-10);
  constraint_factory_.basic().sample_size.SetMax(10);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetMin(20);
  constraint_factory_.basic().sample_size.SetMax(30);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test ideal constraints.
  ResetFactory();
  constraint_factory_.basic().sample_size.SetIdeal(16);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  ResetFactory();
  constraint_factory_.basic().sample_size.SetIdeal(0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
}

TEST_P(MediaStreamConstraintsUtilAudioTest, Channels) {
  int channel_count = kMinChannels;
  AudioCaptureSettings result;

  // Test set exact channelCount.
  for (; channel_count <= media::limits::kMaxChannels; ++channel_count) {
    ResetFactory();
    constraint_factory_.basic().channel_count.SetExact(channel_count);
    result = SelectSettings();

    if (!IsDeviceCapture()) {
      // The source capture configured above is actually using a channel count
      // set to 2 channels.
      if (channel_count <= 2)
        EXPECT_TRUE(result.HasValue());
      else
        EXPECT_FALSE(result.HasValue());
      continue;
    }

    if (channel_count == 3 || channel_count > 4) {
      EXPECT_FALSE(result.HasValue());
      continue;
    }

    EXPECT_TRUE(result.HasValue());
    if (channel_count == 4)
      EXPECT_EQ(result.device_id(), "4_channels_device");
    else
      EXPECT_EQ(result.device_id(), "default_device");
  }

  // Only set a min value for the constraint.
  ResetFactory();
  constraint_factory_.basic().channel_count.SetMin(media::limits::kMaxChannels +
                                                   1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().channel_count.SetMin(kMinChannels);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  // Only set a max value for the constraint.
  ResetFactory();
  constraint_factory_.basic().channel_count.SetMax(kMinChannels - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().channel_count.SetMax(kMinChannels);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  // Define a bounded range for the constraint.
  ResetFactory();
  constraint_factory_.basic().channel_count.SetMin(kMinChannels);
  constraint_factory_.basic().channel_count.SetMax(media::limits::kMaxChannels);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().channel_count.SetMin(kMinChannels - 10);
  constraint_factory_.basic().channel_count.SetMax(kMinChannels - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().channel_count.SetMin(media::limits::kMaxChannels +
                                                   1);
  constraint_factory_.basic().channel_count.SetMax(media::limits::kMaxChannels +
                                                   10);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test ideal constraints.
  for (; channel_count <= media::limits::kMaxChannels; ++channel_count) {
    ResetFactory();
    constraint_factory_.basic().channel_count.SetExact(channel_count);
    result = SelectSettings();

    EXPECT_TRUE(result.HasValue());
    if (IsDeviceCapture()) {
      if (channel_count == 4)
        EXPECT_EQ(result.device_id(), "4_channels_device");
      else
        EXPECT_EQ(result.device_id(), "default_device");
    }
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, ChannelsWithSource) {
  if (!IsDeviceCapture())
    return;

  std::unique_ptr<LocalMediaStreamAudioSource> source =
      GetLocalMediaStreamAudioSource(false /* enable_system_echo_canceller */,
                                     false /* disable_local_echo */,
                                     false /* render_to_associated_sink */);
  int channel_count = kMinChannels;
  for (; channel_count <= media::limits::kMaxChannels; ++channel_count) {
    ResetFactory();
    constraint_factory_.basic().channel_count.SetExact(channel_count);
    auto result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    if (channel_count == 2)
      EXPECT_TRUE(result.HasValue());
    else
      EXPECT_FALSE(result.HasValue());
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, SampleRate) {
  AudioCaptureSettings result;
  int exact_sample_rate = blink::AudioProcessing::kSampleRate8kHz;
  int min_sample_rate = blink::AudioProcessing::kSampleRate8kHz;
  // |max_sample_rate| is different based on architecture, namely due to a
  // difference on Android.
  int max_sample_rate =
      std::max(static_cast<int>(media::AudioParameters::kAudioCDSampleRate),
               blink::kAudioProcessingSampleRate);
  int ideal_sample_rate = blink::AudioProcessing::kSampleRate8kHz;
  if (!IsDeviceCapture()) {
    exact_sample_rate = media::AudioParameters::kAudioCDSampleRate;
    min_sample_rate =
        std::min(static_cast<int>(media::AudioParameters::kAudioCDSampleRate),
                 blink::kAudioProcessingSampleRate);
    ideal_sample_rate = media::AudioParameters::kAudioCDSampleRate;
  }

  // Test set exact sampleRate.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetExact(exact_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().sample_rate.SetExact(11111);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Only set a min value for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetMin(max_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "default_device");

  constraint_factory_.basic().sample_rate.SetMin(max_sample_rate + 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Only set a max value for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetMax(min_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().sample_rate.SetMax(min_sample_rate - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Define a bounded range for the constraint.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetMin(min_sample_rate);
  constraint_factory_.basic().sample_rate.SetMax(max_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "default_device");

  constraint_factory_.basic().sample_rate.SetMin(min_sample_rate - 1000);
  constraint_factory_.basic().sample_rate.SetMax(min_sample_rate - 1);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().sample_rate.SetMin(max_sample_rate + 1);
  constraint_factory_.basic().sample_rate.SetMax(max_sample_rate + 1000);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test ideal constraints.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetIdeal(ideal_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().sample_rate.SetIdeal(ideal_sample_rate);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  if (IsDeviceCapture()) {
    constraint_factory_.basic().sample_rate.SetIdeal(
        blink::AudioProcessing::kSampleRate48kHz + 1000);
    result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.device_id(), "default_device");
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, SampleRateWithSource) {
  if (!IsDeviceCapture())
    return;

  std::unique_ptr<LocalMediaStreamAudioSource> source =
      GetLocalMediaStreamAudioSource(false /* enable_system_echo_canceller */,
                                     false /* disable_local_echo */,
                                     false /* render_to_associated_sink */);

  // Test set exact sampleRate.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetExact(
      media::AudioParameters::kAudioCDSampleRate);
  auto result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().sample_rate.SetExact(11111);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());

  // Test set min sampleRate.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetMin(
      media::AudioParameters::kAudioCDSampleRate);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().sample_rate.SetMin(
      media::AudioParameters::kAudioCDSampleRate + 1);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());

  // Test set max sampleRate.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetMax(
      media::AudioParameters::kAudioCDSampleRate);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().sample_rate.SetMax(
      media::AudioParameters::kAudioCDSampleRate - 1);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());

  // Test set ideal sampleRate.
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetIdeal(
      media::AudioParameters::kAudioCDSampleRate);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().sample_rate.SetIdeal(
      media::AudioParameters::kAudioCDSampleRate - 1);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());
}

TEST_P(MediaStreamConstraintsUtilAudioTest, Latency) {
  // Test set exact sampleRate.
  ResetFactory();
  if (IsDeviceCapture())
    constraint_factory_.basic().latency.SetExact(0.125);
  else
    constraint_factory_.basic().latency.SetExact(0.01);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().latency.SetExact(
      static_cast<double>(blink::kFallbackAudioLatencyMs) / 1000);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "default_device");

  constraint_factory_.basic().latency.SetExact(0.0);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set min sampleRate.
  ResetFactory();
  if (IsDeviceCapture())
    constraint_factory_.basic().latency.SetMin(0.125);
  else
    constraint_factory_.basic().latency.SetMin(0.01);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().latency.SetMin(0.126);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set max sampleRate.
  ResetFactory();
  constraint_factory_.basic().latency.SetMax(0.1);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "default_device");

  constraint_factory_.basic().latency.SetMax(0.001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set bounded sampleRate range.
  ResetFactory();
  if (IsDeviceCapture()) {
    constraint_factory_.basic().latency.SetMin(0.1);
    constraint_factory_.basic().latency.SetMax(0.125);
  } else {
    constraint_factory_.basic().latency.SetMin(0.01);
    constraint_factory_.basic().latency.SetMax(0.1);
  }
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().latency.SetMin(0.0001);
  constraint_factory_.basic().latency.SetMax(0.001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().latency.SetMin(0.126);
  constraint_factory_.basic().latency.SetMax(0.2);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set ideal sampleRate range.
  ResetFactory();
  if (IsDeviceCapture())
    constraint_factory_.basic().latency.SetIdeal(0.125);
  else
    constraint_factory_.basic().latency.SetIdeal(0.01);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "8khz_sample_rate_device");

  constraint_factory_.basic().latency.SetIdeal(0.0);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  if (IsDeviceCapture())
    EXPECT_EQ(result.device_id(), "default_device");
}

TEST_P(MediaStreamConstraintsUtilAudioTest, LatencyWithSource) {
  if (!IsDeviceCapture())
    return;

  std::unique_ptr<LocalMediaStreamAudioSource> source =
      GetLocalMediaStreamAudioSource(false /* enable_system_echo_canceller */,
                                     false /* disable_local_echo */,
                                     false /* render_to_associated_sink */);
  // Test set exact sampleRate.
  ResetFactory();
  constraint_factory_.basic().latency.SetExact(0.01);
  auto result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().latency.SetExact(0.1234);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_FALSE(result.HasValue());

  // Test set min sampleRate.
  ResetFactory();
  constraint_factory_.basic().latency.SetMin(0.01);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().latency.SetMin(0.2);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set max sampleRate.
  ResetFactory();
  constraint_factory_.basic().latency.SetMax(
      static_cast<double>(blink::kFallbackAudioLatencyMs) / 1000);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().latency.SetMax(0.001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set bounded sampleRate range.
  ResetFactory();
  constraint_factory_.basic().latency.SetMin(0.01);
  constraint_factory_.basic().latency.SetMax(0.1);
  result = SelectSettings();
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().latency.SetMin(0.0001);
  constraint_factory_.basic().latency.SetMax(0.001);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  constraint_factory_.basic().latency.SetMin(0.2);
  constraint_factory_.basic().latency.SetMax(0.4);
  result = SelectSettings();
  EXPECT_FALSE(result.HasValue());

  // Test set ideal sampleRate.
  ResetFactory();
  constraint_factory_.basic().latency.SetIdeal(0.01);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());

  constraint_factory_.basic().latency.SetIdeal(0.1234);
  result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());
}

// DeviceID tests.
TEST_P(MediaStreamConstraintsUtilAudioTest, ExactArbitraryDeviceID) {
  const std::string kArbitraryDeviceID = "arbitrary";
  constraint_factory_.basic().device_id.SetExact(
      blink::WebString::FromASCII(kArbitraryDeviceID));
  auto result = SelectSettings();
  // kArbitraryDeviceID is invalid for device capture, but it is considered
  // valid for content capture. For content capture, validation of device
  // capture is performed by the getUserMedia() implementation.
  if (IsDeviceCapture()) {
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(std::string(constraint_factory_.basic().device_id.GetName()),
              std::string(result.failed_constraint_name()));
  } else {
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(kArbitraryDeviceID, result.device_id());
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    CheckEchoCancellationTypeDefault(result);
  }
}

// DeviceID tests check various ways to deal with the device_id constraint.
TEST_P(MediaStreamConstraintsUtilAudioTest, IdealArbitraryDeviceID) {
  const std::string kArbitraryDeviceID = "arbitrary";
  constraint_factory_.basic().device_id.SetIdeal(
      blink::WebString::FromASCII(kArbitraryDeviceID));
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  // kArbitraryDeviceID is invalid for device capture, but it is considered
  // valid for content capture. For content capture, validation of device
  // capture is performed by the getUserMedia() implementation.
  if (IsDeviceCapture())
    CheckDeviceDefaults(result);
  else
    EXPECT_EQ(kArbitraryDeviceID, result.device_id());
  CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                    result);
  CheckEchoCancellationTypeDefault(result);
}

TEST_P(MediaStreamConstraintsUtilAudioTest, ExactValidDeviceID) {
  for (const auto& device : capabilities_) {
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device.DeviceID()));
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(device, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    EchoCancellationType expected_echo_cancellation_type =
        EchoCancellationType::kEchoCancellationDisabled;
    if (IsDeviceCapture()) {
      const bool has_system_echo_cancellation =
          device.Parameters().effects() &
          media::AudioParameters::ECHO_CANCELLER;
      expected_echo_cancellation_type =
          has_system_echo_cancellation
              ? EchoCancellationType::kEchoCancellationSystem
              : EchoCancellationType::kEchoCancellationAec3;
    }
    EXPECT_EQ(expected_echo_cancellation_type,
              result.audio_processing_properties().echo_cancellation_type);
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, ExactGroupID) {
  for (const auto& device : capabilities_) {
    constraint_factory_.basic().group_id.SetExact(
        blink::WebString::FromASCII(device.GroupID()));
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(device, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    EchoCancellationType expected_echo_cancellation_type =
        EchoCancellationType::kEchoCancellationDisabled;
    if (IsDeviceCapture()) {
      const bool has_system_echo_cancellation =
          device.Parameters().effects() &
          media::AudioParameters::ECHO_CANCELLER;
      expected_echo_cancellation_type =
          has_system_echo_cancellation
              ? EchoCancellationType::kEchoCancellationSystem
              : EchoCancellationType::kEchoCancellationAec3;
    }
    EXPECT_EQ(expected_echo_cancellation_type,
              result.audio_processing_properties().echo_cancellation_type);
  }
}

// Tests the echoCancellation constraint with a device without system echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationWithWebRtc) {
  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (bool value : kBoolValues) {
        ResetFactory();
        ((constraint_factory_.*accessor)().echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // With device capture, the echo_cancellation constraint
        // enables/disables all audio processing by default.
        // With content capture, the echo_cancellation constraint controls
        // only the echo_cancellation properties. The other audio processing
        // properties default to false.
        const EchoCancellationType expected_echo_cancellation_type =
            value ? EchoCancellationType::kEchoCancellationAec3
                  : EchoCancellationType::kEchoCancellationDisabled;
        EXPECT_EQ(expected_echo_cancellation_type,
                  properties.echo_cancellation_type);
        const bool enable_webrtc_audio_processing =
            IsDeviceCapture() ? value : false;
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_auto_gain_control);
        CheckGoogExperimentalEchoCancellationDefault(
            properties, enable_webrtc_audio_processing);
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_typing_noise_detection);
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_noise_suppression);
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_experimental_noise_suppression);
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_highpass_filter);
        EXPECT_EQ(enable_webrtc_audio_processing,
                  properties.goog_experimental_auto_gain_control);

        // The following are not audio processing.
        EXPECT_FALSE(properties.goog_audio_mirroring);
        EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
                  result.disable_local_echo());
        EXPECT_FALSE(result.render_to_associated_sink());
        if (IsDeviceCapture()) {
          CheckDevice(*default_device_, result);
        } else {
          EXPECT_TRUE(result.device_id().empty());
        }
      }
    }
  }
}

// Tests the echoCancellation constraint with a device with system echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationWithSystem) {
  // With content capture, there is no system echo cancellation, so
  // nothing to test.
  if (!IsDeviceCapture())
    return;

  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (bool value : kBoolValues) {
        ResetFactory();
        constraint_factory_.basic().device_id.SetExact(
            blink::WebString::FromASCII(
                system_echo_canceller_device_->DeviceID()));
        ((constraint_factory_.*accessor)().echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // With system echo cancellation, the echo_cancellation constraint
        // enables/disables all audio processing by default, WebRTC echo
        // cancellation is always disabled, and system echo cancellation is
        // disabled if the echo_cancellation constraint is false.
        const EchoCancellationType expected_echo_cancellation_type =
            value ? EchoCancellationType::kEchoCancellationSystem
                  : EchoCancellationType::kEchoCancellationDisabled;
        EXPECT_EQ(expected_echo_cancellation_type,
                  properties.echo_cancellation_type);
        EXPECT_EQ(value, properties.goog_auto_gain_control);
        CheckGoogExperimentalEchoCancellationDefault(properties, value);
        EXPECT_EQ(value, properties.goog_typing_noise_detection);
        EXPECT_EQ(value, properties.goog_noise_suppression);
        EXPECT_EQ(value, properties.goog_experimental_noise_suppression);
        EXPECT_EQ(value, properties.goog_highpass_filter);
        EXPECT_EQ(value, properties.goog_experimental_auto_gain_control);

        // The following are not audio processing.
        EXPECT_FALSE(properties.goog_audio_mirroring);
        EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
                  result.disable_local_echo());
        EXPECT_FALSE(result.render_to_associated_sink());
        CheckDevice(*system_echo_canceller_device_, result);
      }
    }
  }
}

// Tests the googEchoCancellation constraint with a device without system echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, GoogEchoCancellationWithWebRtc) {
  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointers due to the comparison failing
      // on compilers.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (bool value : kBoolValues) {
        ResetFactory();
        ((constraint_factory_.*accessor)().goog_echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // The goog_echo_cancellation constraint controls only the
        // echo_cancellation properties. The other audio processing properties
        // use the default values.
        const EchoCancellationType expected_echo_cancellation_type =
            value ? EchoCancellationType::kEchoCancellationAec3
                  : EchoCancellationType::kEchoCancellationDisabled;
        EXPECT_EQ(expected_echo_cancellation_type,
                  properties.echo_cancellation_type);
        CheckBoolDefaults(AudioSettingsBoolMembers(),
                          AudioPropertiesBoolMembers(), result);
        if (IsDeviceCapture()) {
          CheckDevice(*default_device_, result);
        } else {
          EXPECT_TRUE(result.device_id().empty());
        }
      }
    }
  }
}

// Tests the googEchoCancellation constraint with a device with system echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, GoogEchoCancellationWithSystem) {
  // With content capture, there is no system echo cancellation, so
  // nothing to test.
  if (!IsDeviceCapture())
    return;

  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (bool value : kBoolValues) {
        ResetFactory();
        constraint_factory_.basic().device_id.SetExact(
            blink::WebString::FromASCII(
                system_echo_canceller_device_->DeviceID()));
        ((constraint_factory_.*accessor)().goog_echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // With system echo cancellation, WebRTC echo cancellation is always
        // disabled, and system echo cancellation is disabled if
        // goog_echo_cancellation is false.
        const EchoCancellationType expected_echo_cancellation_type =
            value ? EchoCancellationType::kEchoCancellationSystem
                  : EchoCancellationType::kEchoCancellationDisabled;
        EXPECT_EQ(expected_echo_cancellation_type,
                  properties.echo_cancellation_type);
        CheckBoolDefaults(AudioSettingsBoolMembers(),
                          AudioPropertiesBoolMembers(), result);
        CheckDevice(*system_echo_canceller_device_, result);
      }
    }
  }
}

// Tests the echoCancellationType constraint without constraining to a device
// with system echo cancellation. Tested as basic exact constraints.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationTypeExact) {
  for (blink::WebString value : kEchoCancellationTypeValues) {
    ResetFactory();
    constraint_factory_.basic().echo_cancellation.SetExact(true);
    constraint_factory_.basic().echo_cancellation_type.SetExact(value);
    auto result = SelectSettings();
    // If content capture and EC type "system", we expect failure.
    if (!IsDeviceCapture() && value == kEchoCancellationTypeValues[2]) {
      EXPECT_FALSE(result.HasValue());
      EXPECT_EQ(result.failed_constraint_name(),
                constraint_factory_.basic().echo_cancellation_type.GetName());
      continue;
    }
    ASSERT_TRUE(result.HasValue());

    CheckAudioProcessingPropertiesForExactEchoCancellationType(value, result);
  }
}

// Like the test above, but changes the device with system echo cancellation
// support to only support experimental system echo cancellation. It should
// still be picked if requested.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeExact_Experimental) {
  if (!IsDeviceCapture())
    return;

  // Replace the device with one that only supports experimental system echo
  // cancellation.
  MakeSystemEchoCancellerDeviceExperimental();

  for (blink::WebString value : kEchoCancellationTypeValues) {
    ResetFactory();
    constraint_factory_.basic().echo_cancellation.SetExact(true);
    constraint_factory_.basic().echo_cancellation_type.SetExact(value);
    auto result = SelectSettings();
    ASSERT_TRUE(result.HasValue());
    CheckAudioProcessingPropertiesForExactEchoCancellationType(value, result);
  }
}

// Tests the echoCancellationType constraint without constraining to a device
// with system echo cancellation. Tested as basic ideal constraints.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationTypeIdeal) {
  // With content capture, there is no system echo cancellation, so
  // nothing to test.
  if (!IsDeviceCapture())
    return;

  constraint_factory_.basic().echo_cancellation.SetExact(true);
  constraint_factory_.basic().echo_cancellation_type.SetIdeal(
      kEchoCancellationTypeValues[2]);
  auto result = SelectSettings();
  ASSERT_TRUE(result.HasValue());
  CheckAudioProcessingPropertiesForIdealEchoCancellationType(result);
}

// Like the test above, but only having a device with experimental system echo
// cancellation available.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeIdeal_Experimental) {
  // With content capture, there is no system echo cancellation, so
  // nothing to test.
  if (!IsDeviceCapture())
    return;

  // Replace the device with one that only supports experimental system echo
  // cancellation.
  MakeSystemEchoCancellerDeviceExperimental();

  constraint_factory_.basic().echo_cancellation.SetExact(true);
  constraint_factory_.basic().echo_cancellation_type.SetIdeal(
      kEchoCancellationTypeValues[2]);
  auto result = SelectSettings();
  ASSERT_TRUE(result.HasValue());
  CheckAudioProcessingPropertiesForIdealEchoCancellationType(result);
}

// Tests the echoCancellationType constraint with constraining to a device with
// experimental system echo cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeWithExpSystemDeviceConstraint) {
  // With content capture, there is no system echo cancellation, so
  // nothing to test.
  if (!IsDeviceCapture())
    return;

  MakeSystemEchoCancellerDeviceExperimental();

  // Include leaving the echoCancellationType constraint unset in the tests.
  // It should then behave as before the constraint was introduced.
  auto echo_cancellation_types_and_unset = kEchoCancellationTypeValues;
  echo_cancellation_types_and_unset.push_back(blink::WebString());

  for (auto set_function : kStringSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kStringSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (blink::WebString ec_type_value : echo_cancellation_types_and_unset) {
        for (bool ec_value : kBoolValues) {
          ResetFactory();
          constraint_factory_.basic().device_id.SetExact(
              blink::WebString::FromASCII(
                  system_echo_canceller_device_->DeviceID()));
          constraint_factory_.basic().echo_cancellation.SetExact(ec_value);
          if (!ec_type_value.IsNull())
            ((constraint_factory_.*accessor)().echo_cancellation_type.*
             set_function)(ec_type_value);

          // We should get a result if echo cancellation is enabled or if it's
          // disabled and we set the type as an advanced or ideal constraint, or
          // we've left the constraint unset.
          auto result = SelectSettings();
          const bool advanced_constraint = accessor == kFactoryAccessors[1];
          const bool ideal_constraint = set_function == kStringSetFunctions[1];
          const bool should_have_result = ec_value || advanced_constraint ||
                                          ideal_constraint ||
                                          ec_type_value.IsNull();
          EXPECT_EQ(should_have_result, result.HasValue());
          if (!should_have_result)
            continue;

          const AudioProcessingProperties& properties =
              result.audio_processing_properties();

          // With experimental system echo cancellation (echo canceller type
          // "system"), the echo_cancellation constraint enables/disables all
          // audio processing by default, WebRTC echo cancellation is always
          // disabled, and experimental system echo cancellation is disabled
          // if the echo_cancellation constraint is false.
          if (ec_value) {
            const EchoCancellationType expected_echo_cancellation_type =
                ec_type_value == blink::WebString()
                    ? EchoCancellationType::kEchoCancellationAec3
                    : GetEchoCancellationTypeFromConstraintString(
                          ec_type_value);
            EXPECT_EQ(expected_echo_cancellation_type,
                      properties.echo_cancellation_type);
          } else {
            EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
                      properties.echo_cancellation_type);
          }
          EXPECT_EQ(ec_value, properties.goog_auto_gain_control);
          CheckGoogExperimentalEchoCancellationDefault(properties, ec_value);
          EXPECT_EQ(ec_value, properties.goog_typing_noise_detection);
          EXPECT_EQ(ec_value, properties.goog_noise_suppression);
          EXPECT_EQ(ec_value, properties.goog_experimental_noise_suppression);
          EXPECT_EQ(ec_value, properties.goog_highpass_filter);
          EXPECT_EQ(ec_value, properties.goog_experimental_auto_gain_control);

          // The following are not audio processing.
          EXPECT_FALSE(properties.goog_audio_mirroring);
          EXPECT_EQ(GetMediaStreamSource() != blink::kMediaStreamSourceDesktop,
                    result.disable_local_echo());
          EXPECT_FALSE(result.render_to_associated_sink());
          CheckDevice(*system_echo_canceller_device_, result);
        }
      }
    }
  }
}

// Tests the echoCancellationType constraint with constraining to a device
// without experimental system echo cancellation, which should fail.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeWithWebRtcDeviceConstraint) {
  if (!IsDeviceCapture())
    return;

  constraint_factory_.basic().device_id.SetExact(
      blink::WebString::FromASCII(default_device_->DeviceID()));
  constraint_factory_.basic().echo_cancellation.SetExact(true);
  constraint_factory_.basic().echo_cancellation_type.SetExact(
      kEchoCancellationTypeValues[2]);

  auto result = SelectSettings();
  EXPECT_FALSE(result.HasValue());
  EXPECT_EQ(result.failed_constraint_name(),
            constraint_factory_.basic().device_id.GetName());
}

// Test that having differing mandatory values for echoCancellation and
// googEchoCancellation fails.
TEST_P(MediaStreamConstraintsUtilAudioTest, ContradictoryEchoCancellation) {
  for (bool value : kBoolValues) {
    constraint_factory_.basic().echo_cancellation.SetExact(value);
    constraint_factory_.basic().goog_echo_cancellation.SetExact(!value);
    auto result = SelectSettings();
    EXPECT_FALSE(result.HasValue());
    EXPECT_EQ(result.failed_constraint_name(),
              constraint_factory_.basic().echo_cancellation.GetName());
  }
}

// Tests that individual boolean audio-processing constraints override the
// default value set by the echoCancellation constraint.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationAndSingleBoolConstraint) {
  const AudioPropertiesBoolMembers kAudioProcessingProperties = {
      &AudioProcessingProperties::goog_audio_mirroring,
      &AudioProcessingProperties::goog_auto_gain_control,
      &AudioProcessingProperties::goog_experimental_echo_cancellation,
      &AudioProcessingProperties::goog_typing_noise_detection,
      &AudioProcessingProperties::goog_noise_suppression,
      &AudioProcessingProperties::goog_experimental_noise_suppression,
      &AudioProcessingProperties::goog_highpass_filter,
      &AudioProcessingProperties::goog_experimental_auto_gain_control};

  const std::vector<
      blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
      kAudioProcessingConstraints = {
          &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
          &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_echo_cancellation,
          &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
          &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_noise_suppression,
          &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
          &blink::WebMediaTrackConstraintSet::
              goog_experimental_auto_gain_control,
      };

  ASSERT_EQ(kAudioProcessingProperties.size(),
            kAudioProcessingConstraints.size());
  for (auto set_function : kBoolSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kBoolSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      for (size_t i = 0; i < kAudioProcessingProperties.size(); ++i) {
        ResetFactory();
        ((constraint_factory_.*accessor)().echo_cancellation.*
         set_function)(false);
        (((constraint_factory_.*accessor)().*kAudioProcessingConstraints[i]).*
         set_function)(true);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        EXPECT_EQ(EchoCancellationType::kEchoCancellationDisabled,
                  result.audio_processing_properties().echo_cancellation_type);
        EXPECT_TRUE(result.audio_processing_properties().*
                    kAudioProcessingProperties[i]);
        for (size_t j = 0; j < kAudioProcessingProperties.size(); ++j) {
          if (i == j)
            continue;
          EXPECT_FALSE(result.audio_processing_properties().*
                       kAudioProcessingProperties[j]);
        }
      }
    }
  }
}

// Test advanced constraints sets that can be satisfied.
TEST_P(MediaStreamConstraintsUtilAudioTest, AdvancedCompatibleConstraints) {
  constraint_factory_.AddAdvanced().render_to_associated_sink.SetExact(true);
  constraint_factory_.AddAdvanced().goog_audio_mirroring.SetExact(true);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  CheckDeviceDefaults(result);
  CheckBoolDefaults({&AudioCaptureSettings::render_to_associated_sink},
                    {&AudioProcessingProperties::goog_audio_mirroring}, result);
  CheckEchoCancellationTypeDefault(result);
  EXPECT_TRUE(result.render_to_associated_sink());
  EXPECT_TRUE(result.audio_processing_properties().goog_audio_mirroring);
}

// Test that an advanced constraint set that contradicts a previous constraint
// set is ignored, but that further constraint sets that can be satisfied are
// applied.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       AdvancedConflictingMiddleConstraints) {
  constraint_factory_.AddAdvanced().goog_highpass_filter.SetExact(true);
  auto& advanced2 = constraint_factory_.AddAdvanced();
  advanced2.goog_highpass_filter.SetExact(false);
  constraint_factory_.AddAdvanced().goog_audio_mirroring.SetExact(true);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  CheckDeviceDefaults(result);
  CheckBoolDefaults({},
                    {&AudioProcessingProperties::goog_audio_mirroring,
                     &AudioProcessingProperties::goog_highpass_filter},
                    result);
  CheckEchoCancellationTypeDefault(result);
  EXPECT_TRUE(result.audio_processing_properties().goog_audio_mirroring);
  EXPECT_TRUE(result.audio_processing_properties().goog_highpass_filter);
}

// Test that an advanced constraint set that contradicts a previous constraint
// set with a boolean constraint is ignored.
TEST_P(MediaStreamConstraintsUtilAudioTest, AdvancedConflictingLastConstraint) {
  constraint_factory_.AddAdvanced().goog_highpass_filter.SetExact(true);
  constraint_factory_.AddAdvanced().goog_audio_mirroring.SetExact(true);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  CheckDeviceDefaults(result);
  CheckBoolDefaults({},
                    {&AudioProcessingProperties::goog_audio_mirroring,
                     &AudioProcessingProperties::goog_highpass_filter},
                    result);
  CheckEchoCancellationTypeDefault(result);
  // The fourth advanced set is ignored because it contradicts the second set.
  EXPECT_TRUE(result.audio_processing_properties().goog_audio_mirroring);
  EXPECT_TRUE(result.audio_processing_properties().goog_highpass_filter);
}

// NoDevices tests verify that the case with no devices is handled correctly.
TEST_P(MediaStreamConstraintsUtilAudioTest, NoDevicesNoConstraints) {
  // This test makes sense only for device capture.
  if (!IsDeviceCapture())
    return;

  AudioDeviceCaptureCapabilities capabilities;
  auto result = SelectSettingsAudioCapture(
      capabilities, constraint_factory_.CreateWebMediaConstraints(), false);
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name()).empty());
}

TEST_P(MediaStreamConstraintsUtilAudioTest, NoDevicesWithConstraints) {
  // This test makes sense only for device capture.
  if (!IsDeviceCapture())
    return;

  AudioDeviceCaptureCapabilities capabilities;
  constraint_factory_.basic().sample_size.SetExact(16);
  auto result = SelectSettingsAudioCapture(
      capabilities, constraint_factory_.CreateWebMediaConstraints(), false);
  EXPECT_FALSE(result.HasValue());
  EXPECT_TRUE(std::string(result.failed_constraint_name()).empty());
}

// Test functionality to support applyConstraints() for tracks attached to
// sources that have no audio processing.
TEST_P(MediaStreamConstraintsUtilAudioTest, SourceWithNoAudioProcessing) {
  for (bool enable_properties : {true, false}) {
    std::unique_ptr<LocalMediaStreamAudioSource> source =
        GetLocalMediaStreamAudioSource(
            enable_properties /* enable_system_echo_canceller */,
            enable_properties /* disable_local_echo */,
            enable_properties /* render_to_associated_sink */);

    // These constraints are false in |source|.
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kConstraints = {
            &blink::WebMediaTrackConstraintSet::echo_cancellation,
            &blink::WebMediaTrackConstraintSet::disable_local_echo,
            &blink::WebMediaTrackConstraintSet::render_to_associated_sink,
        };

    for (size_t i = 0; i < kConstraints.size(); ++i) {
      constraint_factory_.Reset();
      (constraint_factory_.basic().*kConstraints[i])
          .SetExact(enable_properties);
      auto result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kConstraints[i])
          .SetExact(!enable_properties);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_FALSE(result.HasValue());

      // Setting just ideal values should always succeed.
      constraint_factory_.Reset();
      (constraint_factory_.basic().*kConstraints[i]).SetIdeal(true);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kConstraints[i]).SetIdeal(false);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());
    }
  }
}

// Test functionality to support applyConstraints() for echo cancellation type
// for tracks attached to sources that have no audio processing.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeWithSourceWithNoAudioProcessing) {
  for (blink::WebString value : kEchoCancellationTypeValues) {
    std::unique_ptr<LocalMediaStreamAudioSource> source =
        GetLocalMediaStreamAudioSource(false /* enable_system_echo_canceller */,
                                       false /* disable_local_echo */,
                                       false /* render_to_associated_sink */);

    // No echo cancellation is available so we expect failure.
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation_type.SetExact(value);
    auto result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_FALSE(result.HasValue());

    // Setting just ideal values should always succeed.
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation_type.SetIdeal(value);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());
  }
}

// Test functionality to support applyConstraints() for tracks attached to
// sources that have audio processing.
TEST_P(MediaStreamConstraintsUtilAudioTest, SourceWithAudioProcessing) {
  // Processed audio sources are supported only for device capture.
  if (!IsDeviceCapture())
    return;

  for (bool use_defaults : {true, false}) {
    AudioProcessingProperties properties;
    if (!use_defaults) {
      properties.echo_cancellation_type =
          EchoCancellationType::kEchoCancellationDisabled;
      properties.goog_audio_mirroring = !properties.goog_audio_mirroring;
      properties.goog_auto_gain_control = !properties.goog_auto_gain_control;
      properties.goog_experimental_echo_cancellation =
          !properties.goog_experimental_echo_cancellation;
      properties.goog_typing_noise_detection =
          !properties.goog_typing_noise_detection;
      properties.goog_noise_suppression = !properties.goog_noise_suppression;
      properties.goog_experimental_noise_suppression =
          !properties.goog_experimental_noise_suppression;
      properties.goog_highpass_filter = !properties.goog_highpass_filter;
      properties.goog_experimental_auto_gain_control =
          !properties.goog_experimental_auto_gain_control;
    }

    std::unique_ptr<ProcessedLocalAudioSource> source =
        GetProcessedLocalAudioSource(
            properties, use_defaults /* disable_local_echo */,
            use_defaults /* render_to_associated_sink */);
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kAudioProcessingConstraints = {
            &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
            &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_echo_cancellation,
            &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
            &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_noise_suppression,
            &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_auto_gain_control,
        };
    const AudioPropertiesBoolMembers kAudioProcessingProperties = {
        &AudioProcessingProperties::goog_audio_mirroring,
        &AudioProcessingProperties::goog_auto_gain_control,
        &AudioProcessingProperties::goog_experimental_echo_cancellation,
        &AudioProcessingProperties::goog_typing_noise_detection,
        &AudioProcessingProperties::goog_noise_suppression,
        &AudioProcessingProperties::goog_experimental_noise_suppression,
        &AudioProcessingProperties::goog_highpass_filter,
        &AudioProcessingProperties::goog_experimental_auto_gain_control};

    ASSERT_EQ(kAudioProcessingConstraints.size(),
              kAudioProcessingProperties.size());

    for (size_t i = 0; i < kAudioProcessingConstraints.size(); ++i) {
      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioProcessingConstraints[i])
          .SetExact(properties.*kAudioProcessingProperties[i]);
      auto result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioProcessingConstraints[i])
          .SetExact(!(properties.*kAudioProcessingProperties[i]));
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_FALSE(result.HasValue());

      // Setting just ideal values should always succeed.
      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioProcessingConstraints[i])
          .SetIdeal(true);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioProcessingConstraints[i])
          .SetIdeal(false);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());
    }

    // Test same as above but for echo cancellation.
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(
        properties.echo_cancellation_type ==
        EchoCancellationType::kEchoCancellationAec3);
    auto result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(
        properties.echo_cancellation_type !=
        EchoCancellationType::kEchoCancellationAec3);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_FALSE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetIdeal(true);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetIdeal(false);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());

    // These constraints are false in |source|.
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kAudioBrowserConstraints = {
            &blink::WebMediaTrackConstraintSet::disable_local_echo,
            &blink::WebMediaTrackConstraintSet::render_to_associated_sink,
        };
    for (size_t i = 0; i < kAudioBrowserConstraints.size(); ++i) {
      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioBrowserConstraints[i])
          .SetExact(use_defaults);
      auto result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioBrowserConstraints[i])
          .SetExact(!use_defaults);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_FALSE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioBrowserConstraints[i]).SetIdeal(true);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());

      constraint_factory_.Reset();
      (constraint_factory_.basic().*kAudioBrowserConstraints[i])
          .SetIdeal(false);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());
    }

    // Test same as above for echo cancellation.
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(use_defaults);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(!use_defaults);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_FALSE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetIdeal(true);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());

    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetIdeal(false);
    result = SelectSettingsAudioCapture(
        source.get(), constraint_factory_.CreateWebMediaConstraints());
    EXPECT_TRUE(result.HasValue());
  }
}

// Test functionality to support applyConstraints() for echo cancellation type
// for tracks attached to sources that have audio processing.
TEST_P(MediaStreamConstraintsUtilAudioTest,
       EchoCancellationTypeWithSourceWithAudioProcessing) {
  // Processed audio sources are supported only for device capture.
  if (!IsDeviceCapture())
    return;

  const EchoCancellationType kEchoCancellationTypes[] = {
      EchoCancellationType::kEchoCancellationDisabled,
      EchoCancellationType::kEchoCancellationAec3,
      EchoCancellationType::kEchoCancellationSystem};

  for (EchoCancellationType ec_type : kEchoCancellationTypes) {
    AudioProcessingProperties properties;
    properties.DisableDefaultProperties();
    properties.echo_cancellation_type = ec_type;

    std::unique_ptr<ProcessedLocalAudioSource> source =
        GetProcessedLocalAudioSource(
            properties, false /* disable_local_echo */,
            false /* render_to_associated_sink */,
            ec_type == EchoCancellationType::kEchoCancellationSystem
                ? media::AudioParameters::PlatformEffectsMask::
                      EXPERIMENTAL_ECHO_CANCELLER
                : media::AudioParameters::PlatformEffectsMask::NO_EFFECTS);

    for (blink::WebString value : kEchoCancellationTypeValues) {
      constraint_factory_.Reset();
      constraint_factory_.basic().echo_cancellation_type.SetExact(value);
      auto result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      const bool should_have_result_value =
          ec_type == GetEchoCancellationTypeFromConstraintString(value);
      EXPECT_EQ(should_have_result_value, result.HasValue());

      // Setting just ideal values should always succeed.
      constraint_factory_.Reset();
      constraint_factory_.basic().echo_cancellation_type.SetIdeal(value);
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());
    }
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, UsedAndUnusedSources) {
  // The distinction of used and unused sources is relevant only for device
  // capture.
  if (!IsDeviceCapture())
    return;

  AudioProcessingProperties properties;
  std::unique_ptr<ProcessedLocalAudioSource> processed_source =
      GetProcessedLocalAudioSource(properties, false /* disable_local_echo */,
                                   false /* render_to_associated_sink */);

  const std::string kUnusedDeviceID = "unused_device";
  const std::string kGroupID = "fake_group";
  AudioDeviceCaptureCapabilities capabilities;
  capabilities.emplace_back(processed_source.get());
  capabilities.emplace_back(kUnusedDeviceID, kGroupID,
                            media::AudioParameters::UnavailableDeviceParams());

  {
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(false);

    auto result = SelectSettingsAudioCapture(
        capabilities, constraint_factory_.CreateWebMediaConstraints(),
        false /* should_disable_hardware_noise_suppression */);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.device_id(), kUnusedDeviceID);
    EXPECT_EQ(result.audio_processing_properties().echo_cancellation_type,
              EchoCancellationType::kEchoCancellationDisabled);
  }

  {
    constraint_factory_.Reset();
    constraint_factory_.basic().echo_cancellation.SetExact(true);
    auto result = SelectSettingsAudioCapture(
        capabilities, constraint_factory_.CreateWebMediaConstraints(),
        false /* should_disable_hardware_noise_suppression */);
    EXPECT_TRUE(result.HasValue());
    EXPECT_EQ(result.device_id(), processed_source->device().id);
    EXPECT_EQ(result.audio_processing_properties().echo_cancellation_type,
              EchoCancellationType::kEchoCancellationAec3);
  }
}

TEST_P(MediaStreamConstraintsUtilAudioTest, ExperimetanlEcWithSource) {
  std::unique_ptr<LocalMediaStreamAudioSource> source =
      GetLocalMediaStreamAudioSource(
          false /* enable_system_echo_canceller */,
          false /* disable_local_echo */, false /* render_to_associated_sink */,
          true /* enable_experimental_echo_canceller */);

  constraint_factory_.Reset();
  constraint_factory_.basic().echo_cancellation.SetExact(false);

  auto result = SelectSettingsAudioCapture(
      source.get(), constraint_factory_.CreateWebMediaConstraints());
  EXPECT_TRUE(result.HasValue());
}

TEST_P(MediaStreamConstraintsRemoteAPMTest, Channels) {
  if (!IsDeviceCapture())
    return;

  AudioCaptureSettings result;
  ResetFactory();
  constraint_factory_.basic().channel_count.SetExact(1);
  constraint_factory_.basic().echo_cancellation.SetExact(true);
  result = SelectSettings();

  if (IsApmInAudioServiceEnabled() && GetParam() == true)
    EXPECT_FALSE(result.HasValue());
  else
    EXPECT_TRUE(result.HasValue());
}

TEST_P(MediaStreamConstraintsRemoteAPMTest, SampleRate) {
  if (!IsDeviceCapture())
    return;

  AudioCaptureSettings result;
  ResetFactory();
  constraint_factory_.basic().sample_rate.SetExact(
      media::AudioParameters::kAudioCDSampleRate);
  constraint_factory_.basic().echo_cancellation.SetExact(true);
  result = SelectSettings();

  if (IsApmInAudioServiceEnabled() && GetParam() == true)
    EXPECT_TRUE(result.HasValue());
  else
    EXPECT_FALSE(result.HasValue());
}

TEST_P(MediaStreamConstraintsUtilAudioTest, LatencyConstraint) {
  if (!IsDeviceCapture())
    return;

  // The minimum is 10ms because the AudioParameters used in
  // GetLocalMediaStreamAudioSource() device.input come from the default
  // constructor to blink::MediaStreamDevice, which sets them to
  // AudioParameters::UnavailableDeviceParams(), which uses a 10ms buffer size.
  double default_device_min =
      10 / static_cast<double>(base::Time::kMillisecondsPerSecond);
  double default_device_max =
      1000 / static_cast<double>(media::AudioParameters::kAudioCDSampleRate);

  CheckLatencyConstraint(default_device_, default_device_min,
                         default_device_max);
  CheckLocalMediaStreamAudioSourceLatency(
      default_device_, 0.003,
      default_device_min * media::AudioParameters::kAudioCDSampleRate);
  CheckLocalMediaStreamAudioSourceLatency(
      default_device_, 0.015,
      default_device_min * media::AudioParameters::kAudioCDSampleRate);
  CheckLocalMediaStreamAudioSourceLatency(default_device_, 0.022, 1000);
  CheckLocalMediaStreamAudioSourceLatency(default_device_, 0.04, 1000);

  double variable_latency_device_min =
      128 / static_cast<double>(media::AudioParameters::kAudioCDSampleRate);
  double variable_latency_device_max =
      4096 / static_cast<double>(media::AudioParameters::kAudioCDSampleRate);

  CheckLatencyConstraint(variable_latency_device_, variable_latency_device_min,
                         variable_latency_device_max);

  // Values here are the closest match to the requested latency as returned by
  // media::AudioLatency::GetExactBufferSize().
  CheckLocalMediaStreamAudioSourceLatency(variable_latency_device_, 0.001, 128);
  CheckLocalMediaStreamAudioSourceLatency(variable_latency_device_, 0.011, 512);
#if defined(OS_WIN)
  // Windows only uses exactly the minimum or else multiples of the
  // hardware_buffer_size (512 for the variable_latency_device_).
  CheckLocalMediaStreamAudioSourceLatency(variable_latency_device_, 0.020,
                                          1024);
#else
  CheckLocalMediaStreamAudioSourceLatency(variable_latency_device_, 0.020, 896);
#endif
  CheckLocalMediaStreamAudioSourceLatency(variable_latency_device_, 0.2, 4096);
}

INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamConstraintsUtilAudioTest,
                         testing::Values("",
                                         blink::kMediaStreamSourceTab,
                                         blink::kMediaStreamSourceSystem,
                                         blink::kMediaStreamSourceDesktop));
INSTANTIATE_TEST_SUITE_P(,
                         MediaStreamConstraintsRemoteAPMTest,
                         testing::Bool());

}  // namespace content
