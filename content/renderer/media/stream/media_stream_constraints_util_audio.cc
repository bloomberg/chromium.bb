// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "content/common/media/media_stream_controls.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_constraints_util_sets.h"
#include "content/renderer/media/stream/media_stream_video_source.h"
#include "content/renderer/media/stream/processed_local_audio_source.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "third_party/blink/public/platform/web_media_constraints.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {

using EchoCancellationType = AudioProcessingProperties::EchoCancellationType;
using ConstraintSet = blink::WebMediaTrackConstraintSet;
using StringConstraint = blink::StringConstraint;
using BooleanConstraint = blink::BooleanConstraint;

namespace {

template <class T>
using DiscreteSet = media_constraints::DiscreteSet<T>;

// The presence of a MediaStreamAudioSource object indicates whether the source
// in question is currently in use, or not. This convenience enum helps identify
// whether a source is available and if so whether has audio processing enabled
// or disabled.
enum class SourceType { kNoSource, kUnprocessedSource, kProcessedSource };

// This class encapsulates two values that together build up the score of each
// processed candidate.
// - Fitness, similarly defined by the W3C specification
//   (https://w3c.github.io/mediacapture-main/#dfn-fitness-distance);
// - Distance from the default device ID.
//
// Differently from the definition in the W3C specification, the present
// algorithm maximizes the score.
struct Score {
 public:
  explicit Score(double fitness, bool is_default_device_id = false) {
    score = std::make_tuple(fitness, is_default_device_id);
  }

  bool operator<(const Score& other) const { return score < other.score; }

  Score& operator+=(const Score& other) {
    std::get<0>(score) += std::get<0>(other.score);
    std::get<1>(score) |= std::get<1>(other.score);

    return *this;
  }

  std::tuple<double, bool> score;
};

// Selects the best value from the nonempty |set|, subject to |constraint|. The
// only decision criteria is equality to |constraint.Ideal()|. If there is no
// single best value in |set|, returns nullopt.
base::Optional<bool> SelectOptionalBool(
    const DiscreteSet<bool>& set,
    const blink::BooleanConstraint& constraint) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal()))
    return constraint.Ideal();

  if (set.is_universal())
    return base::Optional<bool>();

  DCHECK_EQ(set.elements().size(), 1U);
  return set.FirstElement();
}

// Container for each independent boolean constrainable property.
class BooleanContainer {
 public:
  BooleanContainer(DiscreteSet<bool> allowed_values = DiscreteSet<bool>())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const BooleanConstraint& constraint) {
    allowed_values_ = allowed_values_.Intersection(
        media_constraints::BoolSetFromConstraint(constraint));
    return allowed_values_.IsEmpty() ? constraint.GetName() : nullptr;
  }

  std::tuple<double, bool> SelectSettingsAndScore(
      const BooleanConstraint& constraint,
      bool default_setting) const {
    DCHECK(!IsEmpty());

    if (constraint.HasIdeal() && allowed_values_.Contains(constraint.Ideal()))
      return std::make_tuple(1.0, constraint.Ideal());

    if (allowed_values_.is_universal())
      return std::make_tuple(0.0, default_setting);

    DCHECK_EQ(allowed_values_.elements().size(), 1U);
    return std::make_tuple(0.0, allowed_values_.FirstElement());
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  DiscreteSet<bool> allowed_values_;
};

// Container for each independent string constrainable property.
class StringContainer {
 public:
  explicit StringContainer(
      DiscreteSet<std::string> allowed_values = DiscreteSet<std::string>())
      : allowed_values_(std::move(allowed_values)) {}

  const char* ApplyConstraintSet(const StringConstraint& constraint) {
    allowed_values_ = allowed_values_.Intersection(
        media_constraints::StringSetFromConstraint(constraint));
    return allowed_values_.IsEmpty() ? constraint.GetName() : nullptr;
  }

  // Selects the best value from the nonempty |allowed_values_|, subject to
  // |constraint_set.*constraint_member_| and determines the associated fitness.
  // The first selection criteria is inclusion in the constraint's ideal value,
  // followed by equality to |default_value|. There is always a single best
  // value.
  std::tuple<double, std::string> SelectSettingsAndScore(
      const StringConstraint& constraint,
      std::string default_setting) const {
    DCHECK(!IsEmpty());
    if (constraint.HasIdeal()) {
      for (const blink::WebString& ideal_candidate : constraint.Ideal()) {
        std::string candidate = ideal_candidate.Utf8();
        if (allowed_values_.Contains(candidate))
          return std::make_tuple(1.0, candidate);
      }
    }

    std::string setting = allowed_values_.Contains(default_setting)
                              ? default_setting
                              : allowed_values_.FirstElement();

    return std::make_tuple(0.0, setting);
  }

  bool IsEmpty() const { return allowed_values_.IsEmpty(); }

 private:
  DiscreteSet<std::string> allowed_values_;
};

// Container to manage the properties related to echo cancellation:
// echoCancellation, googEchoCancellation and echoCancellationType.
class EchoCancellationContainer {
 public:
  EchoCancellationContainer(SourceType source_type,
                            bool is_device_capture,
                            media::AudioParameters parameters,
                            AudioProcessingProperties properties)
      : parameters_(parameters), is_device_capture_(is_device_capture) {
    if (source_type == SourceType::kNoSource) {
      ec_type_allowed_values_ =
          GetEchoCancellationTypesFromParameters(parameters);
      return;
    }

    ec_allowed_values_ = DiscreteSet<bool>({IsEchoCancellationEnabled(
        properties, parameters.effects(),
        source_type == SourceType::kProcessedSource)});
    goog_ec_allowed_values_ = ec_allowed_values_;

    auto type = ToBlinkEchoCancellationType(properties.echo_cancellation_type);
    ec_type_allowed_values_ =
        type ? DiscreteSet<std::string>({*type}) : DiscreteSet<std::string>();
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    ec_allowed_values_ = ec_allowed_values_.Intersection(
        media_constraints::BoolSetFromConstraint(
            constraint_set.echo_cancellation));
    if (ec_allowed_values_.IsEmpty())
      return constraint_set.echo_cancellation.GetName();

    goog_ec_allowed_values_ = goog_ec_allowed_values_.Intersection(
        media_constraints::BoolSetFromConstraint(
            constraint_set.goog_echo_cancellation));
    if (goog_ec_allowed_values_.IsEmpty())
      return constraint_set.goog_echo_cancellation.GetName();

    // Make sure that the results from googEchoCancellation and echoCancellation
    // constraints do not contradict each other.
    auto ec_intersection =
        ec_allowed_values_.Intersection(goog_ec_allowed_values_);
    if (ec_intersection.IsEmpty())
      return constraint_set.echo_cancellation.GetName();

    ec_type_allowed_values_ = ec_type_allowed_values_.Intersection(
        media_constraints::StringSetFromConstraint(
            constraint_set.echo_cancellation_type));
    if (ec_type_allowed_values_.IsEmpty())
      return constraint_set.echo_cancellation_type.GetName();

    // If the echoCancellation constraint is not true, the type set should not
    // have explicit elements, otherwise the two constraints would contradict
    // each other.
    if (!ec_allowed_values_.Contains(true) &&
        constraint_set.echo_cancellation_type.HasExact()) {
      return constraint_set.echo_cancellation_type.GetName();
    }

    return nullptr;
  }

  std::tuple<double, EchoCancellationType> SelectSettingsAndScore(
      const ConstraintSet& constraint_set) const {
    DCHECK(!IsEmpty());
    double fitness = 0.0;
    EchoCancellationType echo_cancellation_type =
        EchoCancellationType::kEchoCancellationDisabled;
    if (ShouldUseEchoCancellation(constraint_set.echo_cancellation,
                                  constraint_set.goog_echo_cancellation)) {
      std::tie(echo_cancellation_type, fitness) =
          SelectEchoCancellationTypeAndFitness(
              constraint_set.echo_cancellation_type);
    }

    return std::make_tuple(fitness, echo_cancellation_type);
  }

  bool IsEmpty() const {
    return ec_allowed_values_.IsEmpty() || goog_ec_allowed_values_.IsEmpty() ||
           ec_type_allowed_values_.IsEmpty();
  }

  // Audio-processing properties are disabled by default for content capture,
  // or if the |echo_cancellation| constraint is false.
  void UpdateDefaultValues(
      const BooleanConstraint& echo_cancellation_constraint,
      AudioProcessingProperties* properties) const {
    auto echo_cancellation =
        SelectOptionalBool(ec_allowed_values_, echo_cancellation_constraint);

    bool default_audio_processing_value = true;
    if (!is_device_capture_ || (echo_cancellation && !*echo_cancellation))
      default_audio_processing_value = false;

    properties->goog_audio_mirroring &= default_audio_processing_value;
    properties->goog_auto_gain_control &= default_audio_processing_value;
    properties->goog_experimental_echo_cancellation &=
        default_audio_processing_value;
    properties->goog_typing_noise_detection &= default_audio_processing_value;
    properties->goog_noise_suppression &= default_audio_processing_value;
    properties->goog_experimental_noise_suppression &=
        default_audio_processing_value;
    properties->goog_highpass_filter &= default_audio_processing_value;
    properties->goog_experimental_auto_gain_control &=
        default_audio_processing_value;
  }

 private:
  static DiscreteSet<std::string> GetEchoCancellationTypesFromParameters(
      const media::AudioParameters& audio_parameters) {
    // The browser and AEC3 echo cancellers are always available.
    std::vector<std::string> types = {blink::kEchoCancellationTypeBrowser,
                                      blink::kEchoCancellationTypeAec3};

    if (audio_parameters.effects() &
        (media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER |
         media::AudioParameters::ECHO_CANCELLER)) {
      // If the system/hardware supports echo cancellation, add it to the set.
      types.push_back(blink::kEchoCancellationTypeSystem);
    }
    return DiscreteSet<std::string>(types);
  }

  static bool IsEchoCancellationEnabled(
      const AudioProcessingProperties& properties,
      int effects,
      bool is_processed) {
    const bool system_echo_cancellation_available =
        (properties.echo_cancellation_type ==
             EchoCancellationType::kEchoCancellationSystem ||
         !is_processed) &&
        effects & media::AudioParameters::ECHO_CANCELLER;

    const bool experimental_system_cancellation_available =
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationSystem &&
        effects & media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;

    return properties.EchoCancellationIsWebRtcProvided() ||
           system_echo_cancellation_available ||
           experimental_system_cancellation_available;
  }

  bool ShouldUseEchoCancellation(
      const BooleanConstraint& echo_cancellation_constraint,
      const BooleanConstraint& goog_echo_cancellation_constraint) const {
    base::Optional<bool> ec =
        SelectOptionalBool(ec_allowed_values_, echo_cancellation_constraint);
    base::Optional<bool> goog_ec = SelectOptionalBool(
        goog_ec_allowed_values_, goog_echo_cancellation_constraint);

    if (ec)
      return *ec;
    if (goog_ec)
      return *goog_ec;

    // Echo cancellation is enabled by default for device capture and disabled
    // by default for content capture.
    return is_device_capture_;
  }

  std::tuple<EchoCancellationType, double> SelectEchoCancellationTypeAndFitness(
      const blink::StringConstraint& echo_cancellation_type) const {
    double fitness = 0.0;

    // Try to use an ideal candidate, if supplied.
    base::Optional<std::string> selected_type;
    if (echo_cancellation_type.HasIdeal()) {
      for (const auto& ideal : echo_cancellation_type.Ideal()) {
        std::string candidate = ideal.Utf8();
        if (ec_type_allowed_values_.Contains(candidate)) {
          selected_type = candidate;
          fitness = 1.0;
          break;
        }
      }
    }

    // If no ideal, or none that worked, and the set contains only one value,
    // pick that.
    if (!selected_type) {
      if (!ec_type_allowed_values_.is_universal() &&
          ec_type_allowed_values_.elements().size() == 1) {
        selected_type = ec_type_allowed_values_.FirstElement();
      }
    }

    // Return type based on the selected type.
    if (selected_type == blink::kEchoCancellationTypeBrowser) {
      return std::make_tuple(EchoCancellationType::kEchoCancellationAec2,
                             fitness);
    } else if (selected_type == blink::kEchoCancellationTypeAec3) {
      return std::make_tuple(EchoCancellationType::kEchoCancellationAec3,
                             fitness);
    } else if (selected_type == blink::kEchoCancellationTypeSystem) {
      return std::make_tuple(EchoCancellationType::kEchoCancellationSystem,
                             fitness);
    }

    // If no type has been selected, choose system if the device has the
    // ECHO_CANCELLER flag set. Never automatically enable an experimental
    // system echo canceller.
    if (parameters_.IsValid() &&
        parameters_.effects() & media::AudioParameters::ECHO_CANCELLER) {
      return std::make_tuple(EchoCancellationType::kEchoCancellationSystem,
                             fitness);
    }

    // Finally, choose the browser provided AEC2 or AEC3 based on an optional
    // override setting for AEC3 or feature.
    // In unit tests not creating a message filter, |aec_dump_message_filter|
    // will be null. We can just ignore that. Other unit tests and browser tests
    // ensure that we do get the filter when we should.
    base::Optional<bool> override_aec3;
    scoped_refptr<AecDumpMessageFilter> aec_dump_message_filter =
        AecDumpMessageFilter::Get();
    if (aec_dump_message_filter)
      override_aec3 = aec_dump_message_filter->GetOverrideAec3();
    const bool use_aec3 = override_aec3.value_or(
        base::FeatureList::IsEnabled(features::kWebRtcUseEchoCanceller3));

    auto ec_type = use_aec3 ? EchoCancellationType::kEchoCancellationAec3
                            : EchoCancellationType::kEchoCancellationAec2;
    return std::make_tuple(ec_type, fitness);
  }

  base::Optional<std::string> ToBlinkEchoCancellationType(
      EchoCancellationType type) const {
    switch (type) {
      case EchoCancellationType::kEchoCancellationAec2:
        return blink::kEchoCancellationTypeBrowser;
      case EchoCancellationType::kEchoCancellationAec3:
        return blink::kEchoCancellationTypeAec3;
      case EchoCancellationType::kEchoCancellationSystem:
        return blink::kEchoCancellationTypeSystem;
      case EchoCancellationType::kEchoCancellationDisabled:
        return base::nullopt;
    }
  }

  DiscreteSet<bool> ec_allowed_values_;
  DiscreteSet<bool> goog_ec_allowed_values_;
  DiscreteSet<std::string> ec_type_allowed_values_;
  media::AudioParameters parameters_;
  bool is_device_capture_;
};

// Container for the constrainable properties of a single audio device.
class DeviceContainer {
 public:
  DeviceContainer(const AudioDeviceCaptureCapability& capability,
                  bool is_device_capture)
      : parameters_(capability.Parameters()) {
    if (!capability.DeviceID().empty())
      device_id_container_ =
          StringContainer(DiscreteSet<std::string>({capability.DeviceID()}));

    if (!capability.GroupID().empty())
      group_id_container_ =
          StringContainer(DiscreteSet<std::string>({capability.GroupID()}));

    // If the device is in use, a source will be provided and all containers
    // must be initialized such that their only supported values correspond to
    // the source settings. Otherwise, the containers are initialized to contain
    // all possible values.
    SourceType source_type;
    AudioProcessingProperties properties;
    std::tie(source_type, properties) = InfoFromSource(capability.source());

    echo_cancellation_container_ = EchoCancellationContainer(
        source_type, is_device_capture, parameters_, properties);

    if (source_type == SourceType::kNoSource)
      return;

    MediaStreamAudioSource* source = capability.source();
    boolean_containers_[kHotwordEnabled] =
        BooleanContainer(DiscreteSet<bool>({source->hotword_enabled()}));

    boolean_containers_[kDisableLocalEcho] =
        BooleanContainer(DiscreteSet<bool>({source->disable_local_echo()}));

    boolean_containers_[kRenderToAssociatedSink] = BooleanContainer(
        DiscreteSet<bool>({source->RenderToAssociatedSinkEnabled()}));

    for (size_t i = kGoogAudioMirroring; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      boolean_containers_[info.index] = BooleanContainer(
          DiscreteSet<bool>({properties.*(info.property_member)}));
    }

    DCHECK(echo_cancellation_container_ != base::nullopt);
    DCHECK_EQ(boolean_containers_.size(), kNumBooleanContainerIds);
#if DCHECK_IS_ON()
    for (const auto& container : boolean_containers_)
      DCHECK(!container.IsEmpty());
#endif
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* failed_constraint_name;

    failed_constraint_name =
        device_id_container_.ApplyConstraintSet(constraint_set.device_id);
    if (failed_constraint_name != nullptr)
      return failed_constraint_name;

    failed_constraint_name =
        group_id_container_.ApplyConstraintSet(constraint_set.group_id);
    if (failed_constraint_name != nullptr)
      return failed_constraint_name;

    for (size_t i = 0; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      failed_constraint_name =
          boolean_containers_[info.index].ApplyConstraintSet(
              constraint_set.*(info.constraint_member));
      if (failed_constraint_name != nullptr)
        return failed_constraint_name;
    }

    failed_constraint_name =
        echo_cancellation_container_->ApplyConstraintSet(constraint_set);
    if (failed_constraint_name != nullptr)
      return failed_constraint_name;

    return nullptr;
  }

  std::tuple<double, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_destkop_source,
      bool should_disable_hardware_noise_suppression,
      std::string default_device_id) const {
    DCHECK(!IsEmpty());
    double score = 0.0;
    double sub_score = 0.0;

    std::string device_id;
    std::tie(sub_score, device_id) =
        device_id_container_.SelectSettingsAndScore(constraint_set.device_id,
                                                    default_device_id);
    score += sub_score;

    std::tie(sub_score, std::ignore) =
        group_id_container_.SelectSettingsAndScore(constraint_set.group_id,
                                                   std::string());
    score += sub_score;

    bool hotword_enabled;
    std::tie(sub_score, hotword_enabled) =
        boolean_containers_[kHotwordEnabled].SelectSettingsAndScore(
            constraint_set.hotword_enabled, false);
    score += sub_score;

    bool disable_local_echo;
    std::tie(sub_score, disable_local_echo) =
        boolean_containers_[kDisableLocalEcho].SelectSettingsAndScore(
            constraint_set.disable_local_echo, !is_destkop_source);
    score += sub_score;

    bool render_to_associated_sink;
    std::tie(sub_score, render_to_associated_sink) =
        boolean_containers_[kRenderToAssociatedSink].SelectSettingsAndScore(
            constraint_set.render_to_associated_sink, false);
    score += sub_score;

    AudioProcessingProperties properties;
    std::tie(sub_score, properties.echo_cancellation_type) =
        echo_cancellation_container_->SelectSettingsAndScore(constraint_set);
    score += sub_score;

    // NOTE: audio-processing properties are disabled by default for content
    // capture, or if the |echo_cancellation| constraint is false. This function
    // call updates the default settings for such properties according to the
    // value obtained for the echo cancellation property.
    echo_cancellation_container_->UpdateDefaultValues(
        constraint_set.echo_cancellation, &properties);

    for (size_t i = kGoogAudioMirroring; i < kNumBooleanContainerIds; ++i) {
      auto& info = kBooleanPropertyContainerInfoMap[i];
      std::tie(sub_score, properties.*(info.property_member)) =
          boolean_containers_[info.index].SelectSettingsAndScore(
              constraint_set.*(info.constraint_member),
              properties.*(info.property_member));
      score += sub_score;
    }

    // NOTE: this is a special case required to support for conditional
    // constraint for echo cancellation type based on an experiment.
    properties.disable_hw_noise_suppression =
        should_disable_hardware_noise_suppression &&
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationDisabled;

    // The score at this point can be considered complete only when the settings
    // are compared against the default device id, which is used as arbitrator
    // in case multiple candidates are available.
    return std::make_tuple(
        score,
        AudioCaptureSettings(device_id, hotword_enabled, disable_local_echo,
                             render_to_associated_sink, properties));
  }

  // The DeviceContainer is considered empty if at least one of the
  // containers owned is empty.
  bool IsEmpty() const {
    DCHECK(!boolean_containers_.empty());

    for (auto& container : boolean_containers_) {
      if (container.IsEmpty())
        return true;
    }

    return device_id_container_.IsEmpty() || group_id_container_.IsEmpty() ||
           echo_cancellation_container_->IsEmpty();
  }

  std::tuple<SourceType, AudioProcessingProperties> InfoFromSource(
      MediaStreamAudioSource* source) {
    AudioProcessingProperties properties;
    SourceType source_type;
    ProcessedLocalAudioSource* processed_source =
        ProcessedLocalAudioSource::From(source);
    if (source == nullptr) {
      source_type = SourceType::kNoSource;
    } else if (processed_source == nullptr) {
      source_type = SourceType::kUnprocessedSource;
      properties.DisableDefaultProperties();
    } else {
      source_type = SourceType::kProcessedSource;
      properties = processed_source->audio_processing_properties();
    }

    return std::make_tuple(source_type, properties);
  }

 private:
  enum StringContainerId { kDeviceId, kGroupId, kNumStringContainerIds };

  enum BooleanContainerId {
    kHotwordEnabled,
    kDisableLocalEcho,
    kRenderToAssociatedSink,
    // Audio processing properties indexes.
    kGoogAudioMirroring,
    kGoogAutoGainControl,
    kGoogExperimentalEchoCancellation,
    kGoogTypingNoiseDetection,
    kGoogNoiseSuppression,
    kGoogExperimentalNoiseSuppression,
    kGoogHighpassFilter,
    kGoogExperimentalAutoGainControl,
    kNumBooleanContainerIds
  };

  // This struct groups related fields or entries from
  // AudioProcessingProperties,
  // SingleDeviceCandidateSet::bool_sets_ and blink::WebMediaTrackConstraintSet.
  struct BooleanPropertyContainerInfo {
    BooleanContainerId index;
    BooleanConstraint ConstraintSet::*constraint_member;
    bool AudioProcessingProperties::*property_member;
  };

  static constexpr BooleanPropertyContainerInfo
      kBooleanPropertyContainerInfoMap[] = {
          {kHotwordEnabled, &ConstraintSet::hotword_enabled, nullptr},
          {kDisableLocalEcho, &ConstraintSet::disable_local_echo, nullptr},
          {kRenderToAssociatedSink, &ConstraintSet::render_to_associated_sink,
           nullptr},
          {kGoogAudioMirroring, &ConstraintSet::goog_audio_mirroring,
           &AudioProcessingProperties::goog_audio_mirroring},
          {kGoogAutoGainControl, &ConstraintSet::goog_auto_gain_control,
           &AudioProcessingProperties::goog_auto_gain_control},
          {kGoogExperimentalEchoCancellation,
           &ConstraintSet::goog_experimental_echo_cancellation,
           &AudioProcessingProperties::goog_experimental_echo_cancellation},
          {kGoogTypingNoiseDetection,
           &ConstraintSet::goog_typing_noise_detection,
           &AudioProcessingProperties::goog_typing_noise_detection},
          {kGoogNoiseSuppression, &ConstraintSet::goog_noise_suppression,
           &AudioProcessingProperties::goog_noise_suppression},
          {kGoogExperimentalNoiseSuppression,
           &ConstraintSet::goog_experimental_noise_suppression,
           &AudioProcessingProperties::goog_experimental_noise_suppression},
          {kGoogHighpassFilter, &ConstraintSet::goog_highpass_filter,
           &AudioProcessingProperties::goog_highpass_filter},
          {kGoogExperimentalAutoGainControl,
           &ConstraintSet::goog_experimental_auto_gain_control,
           &AudioProcessingProperties::goog_experimental_auto_gain_control}};

  media::AudioParameters parameters_;
  StringContainer device_id_container_;
  StringContainer group_id_container_;
  std::array<BooleanContainer, kNumBooleanContainerIds> boolean_containers_;
  base::Optional<EchoCancellationContainer> echo_cancellation_container_;
};

constexpr DeviceContainer::BooleanPropertyContainerInfo
    DeviceContainer::kBooleanPropertyContainerInfoMap[];

// This class represents a set of possible candidate settings.  The
// SelectSettings algorithm starts with a set containing all possible candidates
// based on system/hardware capabilities and/or allowed values for supported
// properties. The set is then reduced progressively as the basic and advanced
// constraint sets are applied. In the end, if the set of candidates is empty,
// SelectSettings fails. If not, the ideal values (if any) or tie breaker rules
// are used to select the final settings based on the candidates that survived
// the application of the constraint sets. This class is implemented as a
// collection of more specific sets for the various supported properties. If any
// of the specific sets is empty, the whole CandidatesContainer is considered
// empty as well.
class CandidatesContainer {
 public:
  CandidatesContainer(const AudioDeviceCaptureCapabilities& capabilities,
                      std::string& media_stream_source,
                      std::string& default_device_id)
      : default_device_id_(default_device_id) {
    for (const auto& capability : capabilities) {
      devices_.emplace_back(capability, media_stream_source.empty());
    }
  }

  const char* ApplyConstraintSet(const ConstraintSet& constraint_set) {
    const char* latest_failed_constraint_name = nullptr;
    for (auto it = devices_.begin(); it != devices_.end();) {
      DCHECK(!it->IsEmpty());
      auto* failed_constraint_name = it->ApplyConstraintSet(constraint_set);
      if (failed_constraint_name) {
        latest_failed_constraint_name = failed_constraint_name;
        devices_.erase(it);
      } else {
        ++it;
      }
    }
    return IsEmpty() ? latest_failed_constraint_name : nullptr;
  }

  std::tuple<Score, AudioCaptureSettings> SelectSettingsAndScore(
      const ConstraintSet& constraint_set,
      bool is_desktop_source,
      bool should_disable_hardware_noise_suppression) const {
    DCHECK(!IsEmpty());
    // Make a copy of the settings initially provided, to track the default
    // settings.
    AudioCaptureSettings best_settings;
    Score best_score(-1.0);
    for (const auto& candidate : devices_) {
      double fitness;
      AudioCaptureSettings settings;
      std::tie(fitness, settings) = candidate.SelectSettingsAndScore(
          constraint_set, is_desktop_source,
          should_disable_hardware_noise_suppression, default_device_id_);

      Score score(fitness, default_device_id_ == settings.device_id());
      if (best_score < score) {
        best_score = score;
        best_settings = std::move(settings);
      }
    }
    return std::make_tuple(best_score, best_settings);
  }

  bool IsEmpty() const { return devices_.empty(); }

 private:
  std::string default_device_id_;
  std::vector<DeviceContainer> devices_;
};

}  // namespace

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability()
    : parameters_(media::AudioParameters::UnavailableDeviceParams()) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    MediaStreamAudioSource* source)
    : source_(source) {}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    std::string device_id,
    std::string group_id,
    const media::AudioParameters& parameters)
    : device_id_(std::move(device_id)),
      group_id_(std::move(group_id)),
      parameters_(parameters) {
  DCHECK(!device_id_.empty());
}

AudioDeviceCaptureCapability::AudioDeviceCaptureCapability(
    const AudioDeviceCaptureCapability& other) = default;

const std::string& AudioDeviceCaptureCapability::DeviceID() const {
  return source_ ? source_->device().id : device_id_;
}

const std::string& AudioDeviceCaptureCapability::GroupID() const {
  return source_ && source_->device().group_id ? *source_->device().group_id
                                               : group_id_;
}

const media::AudioParameters& AudioDeviceCaptureCapability::Parameters() const {
  return source_ ? source_->device().input : parameters_;
}

AudioCaptureSettings SelectSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const blink::WebMediaConstraints& constraints,
    bool should_disable_hardware_noise_suppression) {
  if (capabilities.empty())
    return AudioCaptureSettings();

  std::string media_stream_source = GetMediaStreamSource(constraints);
  std::string default_device_id;
  bool is_device_capture = media_stream_source.empty();
  if (is_device_capture)
    default_device_id = capabilities.begin()->DeviceID();

  CandidatesContainer candidates(capabilities, media_stream_source,
                                 default_device_id);
  auto* failed_constraint_name =
      candidates.ApplyConstraintSet(constraints.Basic());
  if (failed_constraint_name)
    return AudioCaptureSettings(failed_constraint_name);

  for (const auto& advanced_set : constraints.Advanced()) {
    CandidatesContainer copy = candidates;
    failed_constraint_name = candidates.ApplyConstraintSet(advanced_set);
    if (failed_constraint_name)
      candidates = std::move(copy);
  }
  DCHECK(!candidates.IsEmpty());

  // Score is ignored as it is no longer needed.
  AudioCaptureSettings settings;
  std::tie(std::ignore, settings) = candidates.SelectSettingsAndScore(
      constraints.Basic(), media_stream_source == kMediaStreamSourceDesktop,
      should_disable_hardware_noise_suppression);

  return settings;
}

AudioCaptureSettings CONTENT_EXPORT
SelectSettingsAudioCapture(MediaStreamAudioSource* source,
                           const blink::WebMediaConstraints& constraints) {
  DCHECK(source);
  if (source->device().type != MEDIA_DEVICE_AUDIO_CAPTURE &&
      source->device().type != MEDIA_GUM_TAB_AUDIO_CAPTURE &&
      source->device().type != MEDIA_GUM_DESKTOP_AUDIO_CAPTURE) {
    return AudioCaptureSettings();
  }

  std::string media_stream_source = GetMediaStreamSource(constraints);
  if (source->device().type == MEDIA_DEVICE_AUDIO_CAPTURE &&
      !media_stream_source.empty()) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }

  if (source->device().type == MEDIA_GUM_TAB_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != kMediaStreamSourceTab) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }
  if (source->device().type == MEDIA_GUM_DESKTOP_AUDIO_CAPTURE &&
      !media_stream_source.empty() &&
      media_stream_source != kMediaStreamSourceSystem &&
      media_stream_source != kMediaStreamSourceDesktop) {
    return AudioCaptureSettings(
        constraints.Basic().media_stream_source.GetName());
  }

  AudioDeviceCaptureCapabilities capabilities = {
      AudioDeviceCaptureCapability(source)};
  bool should_disable_hardware_noise_suppression =
      !(source->device().input.effects() &
        media::AudioParameters::NOISE_SUPPRESSION);

  return SelectSettingsAudioCapture(capabilities, constraints,
                                    should_disable_hardware_noise_suppression);
}

}  // namespace content
