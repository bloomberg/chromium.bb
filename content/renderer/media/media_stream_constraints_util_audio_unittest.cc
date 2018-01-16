// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util_audio.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/message_loop/message_loop.h"
#include "content/common/media/media_stream_controls.h"
#include "content/renderer/media/local_media_stream_audio_source.h"
#include "content/renderer/media/media_stream_audio_source.h"
#include "content/renderer/media/media_stream_source.h"
#include "content/renderer/media/mock_constraint_factory.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/processed_local_audio_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/modules/mediastream/media_devices.mojom.h"

namespace content {

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

using AudioSettingsBoolMembers =
    std::vector<bool (AudioCaptureSettings::*)() const>;
using AudioPropertiesBoolMembers =
    std::vector<bool AudioProcessingProperties::*>;

template <typename T>
static bool Contains(const std::vector<T>& vector, T value) {
  auto it = std::find(vector.begin(), vector.end(), value);
  return it != vector.end();
}

}  // namespace

class MediaStreamConstraintsUtilAudioTest
    : public testing::TestWithParam<std::string> {
 public:
  void SetUp() override {
    ResetFactory();
    if (!IsDeviceCapture())
      return;

    blink::mojom::AudioInputDeviceCapabilitiesPtr device =
        blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = "default_device";
    device->parameters = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16, 1000);
    capabilities_.push_back(std::move(device));

    device = blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = "hw_echo_canceller_device";
    device->parameters = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 24, 1000);
    device->parameters.set_effects(media::AudioParameters::ECHO_CANCELLER);
    capabilities_.push_back(std::move(device));

    device = blink::mojom::AudioInputDeviceCapabilities::New();
    device->device_id = "geometry device";
    device->parameters = media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::CHANNEL_LAYOUT_STEREO,
        media::AudioParameters::kAudioCDSampleRate, 16, 1000);
    device->parameters.set_mic_positions(kMicPositions);
    capabilities_.push_back(std::move(device));

    default_device_ = capabilities_[0].get();
    hw_echo_canceller_device_ = capabilities_[1].get();
    geometry_device_ = capabilities_[2].get();
  }

 protected:
  void SetMediaStreamSource(const std::string& source) {}

  void ResetFactory() {
    constraint_factory_.Reset();
    constraint_factory_.basic().media_stream_source.SetExact(
        blink::WebString::FromASCII(GetParam()));
  }

  std::string GetMediaStreamSource() { return GetParam(); }
  bool IsDeviceCapture() { return GetMediaStreamSource().empty(); }

  MediaStreamType GetMediaStreamType() {
    std::string media_source = GetMediaStreamSource();
    if (media_source.empty())
      return MEDIA_DEVICE_AUDIO_CAPTURE;
    else if (media_source == kMediaStreamSourceTab)
      return MEDIA_TAB_AUDIO_CAPTURE;
    return MEDIA_DESKTOP_AUDIO_CAPTURE;
  }

  std::unique_ptr<ProcessedLocalAudioSource> GetProcessedLocalAudioSource(
      const AudioProcessingProperties& properties,
      bool hotword_enabled,
      bool disable_local_echo,
      bool render_to_associated_sink) {
    MediaStreamDevice device;
    device.type = GetMediaStreamType();
    if (render_to_associated_sink)
      device.matched_output_device_id = std::string("some_device_id");

    return std::make_unique<ProcessedLocalAudioSource>(
        -1, device, hotword_enabled, disable_local_echo, properties,
        MediaStreamSource::ConstraintsCallback(), &pc_factory_);
  }

  std::unique_ptr<LocalMediaStreamAudioSource> GetLocalMediaStreamAudioSource(
      bool enable_hardware_echo_canceller,
      bool hotword_enabled,
      bool disable_local_echo,
      bool render_to_associated_sink) {
    MediaStreamDevice device;
    device.type = GetMediaStreamType();
    if (enable_hardware_echo_canceller)
      device.input.set_effects(media::AudioParameters::ECHO_CANCELLER);
    if (render_to_associated_sink)
      device.matched_output_device_id = std::string("some_device_id");

    return std::make_unique<LocalMediaStreamAudioSource>(
        -1, device, hotword_enabled, disable_local_echo,
        MediaStreamSource::ConstraintsCallback());
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
                  &AudioCaptureSettings::hotword_enabled)) {
      EXPECT_FALSE(result.hotword_enabled());
    }
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
                  &AudioProcessingProperties::enable_sw_echo_cancellation)) {
      EXPECT_TRUE(properties.enable_sw_echo_cancellation);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::disable_hw_echo_cancellation)) {
      EXPECT_FALSE(properties.disable_hw_echo_cancellation);
    }
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
                  &AudioProcessingProperties::goog_beamforming)) {
      EXPECT_TRUE(properties.goog_beamforming);
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
                  &AudioCaptureSettings::hotword_enabled)) {
      EXPECT_FALSE(result.hotword_enabled());
    }
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::disable_local_echo)) {
      EXPECT_EQ(GetMediaStreamSource() != kMediaStreamSourceDesktop,
                result.disable_local_echo());
    }
    if (!Contains(exclude_main_settings,
                  &AudioCaptureSettings::render_to_associated_sink)) {
      EXPECT_FALSE(result.render_to_associated_sink());
    }

    const auto& properties = result.audio_processing_properties();
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::enable_sw_echo_cancellation)) {
      EXPECT_FALSE(properties.enable_sw_echo_cancellation);
    }
    if (!Contains(exclude_audio_properties,
                  &AudioProcessingProperties::disable_hw_echo_cancellation)) {
      EXPECT_FALSE(properties.disable_hw_echo_cancellation);
    }
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
                  &AudioProcessingProperties::goog_beamforming)) {
      EXPECT_FALSE(properties.goog_beamforming);
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

  void CheckDevice(
      const blink::mojom::AudioInputDeviceCapabilities& expected_device,
      const AudioCaptureSettings& result) {
    EXPECT_EQ(expected_device.device_id, result.device_id());
    EXPECT_EQ(expected_device.parameters.sample_rate(),
              result.device_parameters().sample_rate());
    EXPECT_EQ(expected_device.parameters.bits_per_sample(),
              result.device_parameters().bits_per_sample());
    EXPECT_EQ(expected_device.parameters.channels(),
              result.device_parameters().channels());
    EXPECT_EQ(expected_device.parameters.effects(),
              result.device_parameters().effects());
  }

  void CheckDeviceDefaults(const AudioCaptureSettings& result) {
    if (IsDeviceCapture())
      CheckDevice(*default_device_, result);
    else
      EXPECT_TRUE(result.device_id().empty());
  }

  void CheckGeometryDefaults(const AudioCaptureSettings& result) {
    EXPECT_TRUE(
        result.audio_processing_properties().goog_array_geometry.empty());
  }

  void CheckAllDefaults(
      const AudioSettingsBoolMembers& exclude_main_settings,
      const AudioPropertiesBoolMembers& exclude_audio_properties,
      const AudioCaptureSettings& result) {
    CheckBoolDefaults(exclude_main_settings, exclude_audio_properties, result);
    CheckDeviceDefaults(result);
    CheckGeometryDefaults(result);
  }

  MockConstraintFactory constraint_factory_;
  AudioDeviceCaptureCapabilities capabilities_;
  const blink::mojom::AudioInputDeviceCapabilities* default_device_ = nullptr;
  const blink::mojom::AudioInputDeviceCapabilities* hw_echo_canceller_device_ =
      nullptr;
  const blink::mojom::AudioInputDeviceCapabilities* geometry_device_ = nullptr;
  const std::vector<media::Point> kMicPositions = {{8, 8, 8}, {4, 4, 4}};

 private:
  // Required for tests involving a MediaStreamAudioSource.
  base::MessageLoop message_loop_;
  MockPeerConnectionDependencyFactory pc_factory_;
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
  // TODO(crbug.com/736309): Use braced initialization instead of push_back once
  // clang has been fixed.
  AudioSettingsBoolMembers kMainSettings;
  kMainSettings.push_back(&AudioCaptureSettings::hotword_enabled);
  kMainSettings.push_back(&AudioCaptureSettings::disable_local_echo);
  kMainSettings.push_back(&AudioCaptureSettings::render_to_associated_sink);

  const std::vector<
      blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
      kMainBoolConstraints = {
          &blink::WebMediaTrackConstraintSet::hotword_enabled,
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
      &AudioProcessingProperties::goog_beamforming,
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
          &blink::WebMediaTrackConstraintSet::goog_beamforming,
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
    CheckGeometryDefaults(result);
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
  CheckGeometryDefaults(result);
}

TEST_P(MediaStreamConstraintsUtilAudioTest, ExactValidDeviceID) {
  for (const auto& device : capabilities_) {
    constraint_factory_.basic().device_id.SetExact(
        blink::WebString::FromASCII(device->device_id));
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(*device, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(),
                      {&AudioProcessingProperties::enable_sw_echo_cancellation},
                      result);
    bool has_hw_echo_cancellation =
        device->parameters.effects() & media::AudioParameters::ECHO_CANCELLER;
    EXPECT_EQ(!has_hw_echo_cancellation,
              result.audio_processing_properties().enable_sw_echo_cancellation);
    if (&*device == geometry_device_) {
      EXPECT_EQ(kMicPositions,
                result.audio_processing_properties().goog_array_geometry);
    } else {
      CheckGeometryDefaults(result);
    }
  }
}

// Tests the echoCancellation constraint with a device without hardware echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationWithSw) {
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
        bool enable_sw_audio_processing = IsDeviceCapture() ? value : false;
        EXPECT_EQ(value, properties.enable_sw_echo_cancellation);
        EXPECT_EQ(!value, properties.disable_hw_echo_cancellation);
        EXPECT_EQ(enable_sw_audio_processing,
                  properties.goog_auto_gain_control);
        CheckGoogExperimentalEchoCancellationDefault(
            properties, enable_sw_audio_processing);
        EXPECT_EQ(enable_sw_audio_processing,
                  properties.goog_typing_noise_detection);
        EXPECT_EQ(enable_sw_audio_processing,
                  properties.goog_noise_suppression);
        EXPECT_EQ(enable_sw_audio_processing,
                  properties.goog_experimental_noise_suppression);
        EXPECT_EQ(enable_sw_audio_processing, properties.goog_beamforming);
        EXPECT_EQ(enable_sw_audio_processing, properties.goog_highpass_filter);
        EXPECT_EQ(enable_sw_audio_processing,
                  properties.goog_experimental_auto_gain_control);

        // The following are not audio processing.
        EXPECT_FALSE(properties.goog_audio_mirroring);
        EXPECT_FALSE(result.hotword_enabled());
        EXPECT_EQ(GetMediaStreamSource() != kMediaStreamSourceDesktop,
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

// Tests the echoCancellation constraint with a device with hardware echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, EchoCancellationWithHw) {
  // With content capture, there is no hardware echo cancellation, so
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
            blink::WebString::FromASCII(hw_echo_canceller_device_->device_id));
        ((constraint_factory_.*accessor)().echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // With hardware echo cancellation, the echo_cancellation constraint
        // enables/disables all audio processing by default, software echo
        // cancellation is always disabled, and hardware echo cancellation is
        // disabled if the echo_cancellation constraint is false.
        EXPECT_FALSE(properties.enable_sw_echo_cancellation);
        EXPECT_EQ(!value, properties.disable_hw_echo_cancellation);
        EXPECT_EQ(value, properties.goog_auto_gain_control);
        CheckGoogExperimentalEchoCancellationDefault(properties, value);
        EXPECT_EQ(value, properties.goog_typing_noise_detection);
        EXPECT_EQ(value, properties.goog_noise_suppression);
        EXPECT_EQ(value, properties.goog_experimental_noise_suppression);
        EXPECT_EQ(value, properties.goog_beamforming);
        EXPECT_EQ(value, properties.goog_highpass_filter);
        EXPECT_EQ(value, properties.goog_experimental_auto_gain_control);

        // The following are not audio processing.
        EXPECT_FALSE(properties.goog_audio_mirroring);
        EXPECT_FALSE(result.hotword_enabled());
        EXPECT_EQ(GetMediaStreamSource() != kMediaStreamSourceDesktop,
                  result.disable_local_echo());
        EXPECT_FALSE(result.render_to_associated_sink());
        CheckDevice(*hw_echo_canceller_device_, result);
      }
    }
  }
}

// Tests the googEchoCancellation constraint with a device without hardware echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, GoogEchoCancellationWithSw) {
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
        EXPECT_EQ(value, properties.enable_sw_echo_cancellation);
        EXPECT_EQ(!value, properties.disable_hw_echo_cancellation);
        CheckBoolDefaults(
            AudioSettingsBoolMembers(),
            {
                &AudioProcessingProperties::enable_sw_echo_cancellation,
                &AudioProcessingProperties::disable_hw_echo_cancellation,
            },
            result);
        if (IsDeviceCapture()) {
          CheckDevice(*default_device_, result);
        } else {
          EXPECT_TRUE(result.device_id().empty());
        }
      }
    }
  }
}

// Tests the googEchoCancellation constraint with a device without hardware echo
// cancellation.
TEST_P(MediaStreamConstraintsUtilAudioTest, GoogEchoCancellationWithHw) {
  // With content capture, there is no hardware echo cancellation, so
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
            blink::WebString::FromASCII(hw_echo_canceller_device_->device_id));
        ((constraint_factory_.*accessor)().goog_echo_cancellation.*
         set_function)(value);
        auto result = SelectSettings();
        EXPECT_TRUE(result.HasValue());
        const AudioProcessingProperties& properties =
            result.audio_processing_properties();
        // With hardware echo cancellation, software echo cancellation is always
        // disabled, and hardware echo cancellation is disabled if
        // goog_echo_cancellation is false.
        EXPECT_FALSE(properties.enable_sw_echo_cancellation);
        EXPECT_EQ(!value, properties.disable_hw_echo_cancellation);
        CheckBoolDefaults(
            AudioSettingsBoolMembers(),
            {
                &AudioProcessingProperties::enable_sw_echo_cancellation,
                &AudioProcessingProperties::disable_hw_echo_cancellation,
            },
            result);
        CheckDevice(*hw_echo_canceller_device_, result);
      }
    }
  }
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
      &AudioProcessingProperties::goog_beamforming,
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
          &blink::WebMediaTrackConstraintSet::goog_beamforming,
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
        EXPECT_FALSE(
            result.audio_processing_properties().enable_sw_echo_cancellation);
        EXPECT_TRUE(
            result.audio_processing_properties().disable_hw_echo_cancellation);
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
  // TODO(crbug.com/736309): Fold this local when clang is fixed.
  auto render_to_associated_sink =
      &AudioCaptureSettings::render_to_associated_sink;
  CheckBoolDefaults({render_to_associated_sink},
                    {&AudioProcessingProperties::goog_audio_mirroring}, result);
  CheckGeometryDefaults(result);
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
  advanced2.hotword_enabled.SetExact(true);
  constraint_factory_.AddAdvanced().goog_audio_mirroring.SetExact(true);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  CheckDeviceDefaults(result);
  EXPECT_FALSE(result.hotword_enabled());
  // TODO(crbug.com/736309): Fold this local when clang is fixed.
  auto hotword_enabled = &AudioCaptureSettings::hotword_enabled;
  CheckBoolDefaults({hotword_enabled},
                    {&AudioProcessingProperties::goog_audio_mirroring,
                     &AudioProcessingProperties::goog_highpass_filter},
                    result);
  CheckGeometryDefaults(result);
  EXPECT_FALSE(result.hotword_enabled());
  EXPECT_TRUE(result.audio_processing_properties().goog_audio_mirroring);
  EXPECT_TRUE(result.audio_processing_properties().goog_highpass_filter);
}

// Test that an advanced constraint set that contradicts a previous constraint
// set with a boolean constraint is ignored.
TEST_P(MediaStreamConstraintsUtilAudioTest, AdvancedConflictingLastConstraint) {
  constraint_factory_.AddAdvanced().goog_highpass_filter.SetExact(true);
  constraint_factory_.AddAdvanced().hotword_enabled.SetExact(true);
  constraint_factory_.AddAdvanced().goog_audio_mirroring.SetExact(true);
  constraint_factory_.AddAdvanced().hotword_enabled.SetExact(false);
  auto result = SelectSettings();
  EXPECT_TRUE(result.HasValue());
  CheckDeviceDefaults(result);
  // TODO(crbug.com/736309): Fold this local when clang is fixed.
  auto hotword_enabled = &AudioCaptureSettings::hotword_enabled;
  CheckBoolDefaults({hotword_enabled},
                    {&AudioProcessingProperties::goog_audio_mirroring,
                     &AudioProcessingProperties::goog_highpass_filter},
                    result);
  CheckGeometryDefaults(result);
  // The fourth advanced set is ignored because it contradicts the second set.
  EXPECT_TRUE(result.hotword_enabled());
  EXPECT_TRUE(result.audio_processing_properties().goog_audio_mirroring);
  EXPECT_TRUE(result.audio_processing_properties().goog_highpass_filter);
}

// Test that a valid geometry is interpreted correctly in all the ways it can
// be set.
TEST_P(MediaStreamConstraintsUtilAudioTest, ValidGeometry) {
  const blink::WebString kGeometry =
      blink::WebString::FromASCII("-0.02 0 0 0.02 0 1.01");

  for (auto set_function : kStringSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kStringSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      ResetFactory();
      ((constraint_factory_.*accessor)().goog_array_geometry.*
       set_function)(kGeometry);
      auto result = SelectSettings();
      EXPECT_TRUE(result.HasValue());
      CheckDeviceDefaults(result);
      CheckBoolDefaults(AudioSettingsBoolMembers(),
                        AudioPropertiesBoolMembers(), result);
      const std::vector<media::Point>& geometry =
          result.audio_processing_properties().goog_array_geometry;
      EXPECT_EQ(2u, geometry.size());
      EXPECT_EQ(media::Point(-0.02, 0, 0), geometry[0]);
      EXPECT_EQ(media::Point(0.02, 0, 1.01), geometry[1]);
    }
  }
}

// Test that an invalid geometry is interpreted as empty in all the ways it can
// be set.
TEST_P(MediaStreamConstraintsUtilAudioTest, InvalidGeometry) {
  const blink::WebString kGeometry =
      blink::WebString::FromASCII("1 1 1 invalid");

  for (auto set_function : kStringSetFunctions) {
    for (auto accessor : kFactoryAccessors) {
      // Ideal advanced is ignored by the SelectSettings algorithm.
      // Using array elements instead of pointer values due to the comparison
      // failing on some build configurations.
      if (set_function == kStringSetFunctions[1] &&
          accessor == kFactoryAccessors[1]) {
        continue;
      }
      ResetFactory();
      ((constraint_factory_.*accessor)().goog_array_geometry.*
       set_function)(kGeometry);
      auto result = SelectSettings();
      EXPECT_TRUE(result.HasValue());
      CheckDeviceDefaults(result);
      CheckBoolDefaults(AudioSettingsBoolMembers(),
                        AudioPropertiesBoolMembers(), result);
      const std::vector<media::Point>& geometry =
          result.audio_processing_properties().goog_array_geometry;
      EXPECT_EQ(0u, geometry.size());
    }
  }
}

// Test that an invalid geometry is interpreted as empty in all the ways it can
// be set.
TEST_P(MediaStreamConstraintsUtilAudioTest, DeviceGeometry) {
  if (!IsDeviceCapture())
    return;

  constraint_factory_.basic().device_id.SetExact(
      blink::WebString::FromASCII(geometry_device_->device_id));

  {
    const blink::WebString kValidGeometry =
        blink::WebString::FromASCII("-0.02 0 0  0.02 0 1.01");
    constraint_factory_.basic().goog_array_geometry.SetExact(kValidGeometry);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(*geometry_device_, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    // Constraints geometry should be preferred over device geometry.
    const std::vector<media::Point>& geometry =
        result.audio_processing_properties().goog_array_geometry;
    EXPECT_EQ(2u, geometry.size());
    EXPECT_EQ(media::Point(-0.02, 0, 0), geometry[0]);
    EXPECT_EQ(media::Point(0.02, 0, 1.01), geometry[1]);
  }

  {
    constraint_factory_.basic().goog_array_geometry.SetExact(
        blink::WebString());
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(*geometry_device_, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    // Empty geometry is valid and should be preferred over device geometry.
    EXPECT_TRUE(
        result.audio_processing_properties().goog_array_geometry.empty());
  }

  {
    const blink::WebString kInvalidGeometry =
        blink::WebString::FromASCII("1 1 1 invalid");
    constraint_factory_.basic().goog_array_geometry.SetExact(kInvalidGeometry);
    auto result = SelectSettings();
    EXPECT_TRUE(result.HasValue());
    CheckDevice(*geometry_device_, result);
    CheckBoolDefaults(AudioSettingsBoolMembers(), AudioPropertiesBoolMembers(),
                      result);
    // Device geometry should be preferred over invalid constraints geometry.
    EXPECT_EQ(kMicPositions,
              result.audio_processing_properties().goog_array_geometry);
  }
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
            enable_properties /* enable_hardware_echo_canceller */,
            enable_properties /* hotword_enabled */,
            enable_properties /* disable_local_echo */,
            enable_properties /* render_to_associated_sink */);

    // These constraints are false in |source|.
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kConstraints = {
            &blink::WebMediaTrackConstraintSet::echo_cancellation,
            &blink::WebMediaTrackConstraintSet::hotword_enabled,
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

// Test functionality to support applyConstraints() for tracks attached to
// sources that have audio processing.
TEST_P(MediaStreamConstraintsUtilAudioTest, SourceWithAudioProcessing) {
  // Processed audio sources are supported only for device capture.
  if (!IsDeviceCapture())
    return;

  for (bool use_defaults : {true, false}) {
    AudioProcessingProperties properties;
    properties.disable_hw_echo_cancellation = true;
    if (!use_defaults) {
      properties.enable_sw_echo_cancellation =
          !properties.enable_sw_echo_cancellation;
      properties.goog_audio_mirroring = !properties.goog_audio_mirroring;
      properties.goog_auto_gain_control = !properties.goog_auto_gain_control;
      properties.goog_experimental_echo_cancellation =
          !properties.goog_experimental_echo_cancellation;
      properties.goog_typing_noise_detection =
          !properties.goog_typing_noise_detection;
      properties.goog_noise_suppression = !properties.goog_noise_suppression;
      properties.goog_experimental_noise_suppression =
          !properties.goog_experimental_noise_suppression;
      properties.goog_beamforming = !properties.goog_beamforming;
      properties.goog_highpass_filter = !properties.goog_highpass_filter;
      properties.goog_experimental_auto_gain_control =
          !properties.goog_experimental_auto_gain_control;
      properties.goog_array_geometry = {{1, 1, 1}, {2, 2, 2}};
    }

    std::unique_ptr<ProcessedLocalAudioSource> source =
        GetProcessedLocalAudioSource(
            properties, use_defaults /* hotword_enabled */,
            use_defaults /* disable_local_echo */,
            use_defaults /* render_to_associated_sink */);
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kAudioProcessingConstraints = {
            &blink::WebMediaTrackConstraintSet::echo_cancellation,
            &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
            &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_echo_cancellation,
            &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
            &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_noise_suppression,
            &blink::WebMediaTrackConstraintSet::goog_beamforming,
            &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
            &blink::WebMediaTrackConstraintSet::
                goog_experimental_auto_gain_control,
        };
    const AudioPropertiesBoolMembers kAudioProcessingProperties = {
        &AudioProcessingProperties::enable_sw_echo_cancellation,
        &AudioProcessingProperties::goog_audio_mirroring,
        &AudioProcessingProperties::goog_auto_gain_control,
        &AudioProcessingProperties::goog_experimental_echo_cancellation,
        &AudioProcessingProperties::goog_typing_noise_detection,
        &AudioProcessingProperties::goog_noise_suppression,
        &AudioProcessingProperties::goog_experimental_noise_suppression,
        &AudioProcessingProperties::goog_beamforming,
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

    {
      constraint_factory_.Reset();
      // TODO(guidou): Support any semantically equivalent strings for the
      // goog_array_geometry constrainable property. http://crbug.com/796955
      constraint_factory_.basic().goog_array_geometry.SetExact(
          blink::WebString("1 1 1  2 2 2"));
      auto result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_EQ(result.HasValue(), !use_defaults);

      // Setting ideal should succeed.
      constraint_factory_.Reset();
      constraint_factory_.basic().goog_array_geometry.SetIdeal(
          blink::WebString("4 4 4  5 5 5"));
      result = SelectSettingsAudioCapture(
          source.get(), constraint_factory_.CreateWebMediaConstraints());
      EXPECT_TRUE(result.HasValue());
    }

    // These constraints are false in |source|.
    const std::vector<
        blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*>
        kAudioBrowserConstraints = {
            &blink::WebMediaTrackConstraintSet::hotword_enabled,
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
  }
}

INSTANTIATE_TEST_CASE_P(,
                        MediaStreamConstraintsUtilAudioTest,
                        testing::Values("",
                                        kMediaStreamSourceTab,
                                        kMediaStreamSourceSystem,
                                        kMediaStreamSourceDesktop));

}  // namespace content
