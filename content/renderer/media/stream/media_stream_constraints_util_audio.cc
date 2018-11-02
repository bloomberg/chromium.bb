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

namespace {

template <class T>
using DiscreteSet = media_constraints::DiscreteSet<T>;

enum BoolConstraint {
  // Constraints not related to audio processing.
  HOTWORD_ENABLED,
  DISABLE_LOCAL_ECHO,
  RENDER_TO_ASSOCIATED_SINK,

  // Constraints that enable/disable audio processing.
  ECHO_CANCELLATION,
  GOOG_ECHO_CANCELLATION,

  // Constraints that control audio-processing behavior.
  GOOG_AUDIO_MIRRORING,
  GOOG_AUTO_GAIN_CONTROL,
  GOOG_EXPERIMENTAL_ECHO_CANCELLATION,
  GOOG_TYPING_NOISE_DETECTION,
  GOOG_NOISE_SUPPRESSION,
  GOOG_EXPERIMENTAL_NOISE_SUPPRESSION,
  GOOG_HIGHPASS_FILTER,
  GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL,
  NUM_BOOL_CONSTRAINTS
};

// This struct groups related fields or entries from AudioProcessingProperties,
// SingleDeviceCandidateSet::bool_sets_ and blink::WebMediaTrackConstraintSet.
struct AudioPropertyConstraintPair {
  bool AudioProcessingProperties::* audio_property;
  BoolConstraint bool_set_index;
};

// Boolean audio properties that are mapped directly to a boolean constraint
// and which are subject to the same processing.
const AudioPropertyConstraintPair kAudioPropertyConstraintMap[] = {
    {&AudioProcessingProperties::goog_auto_gain_control,
     GOOG_AUTO_GAIN_CONTROL},
    {&AudioProcessingProperties::goog_experimental_echo_cancellation,
     GOOG_EXPERIMENTAL_ECHO_CANCELLATION},
    {&AudioProcessingProperties::goog_typing_noise_detection,
     GOOG_TYPING_NOISE_DETECTION},
    {&AudioProcessingProperties::goog_noise_suppression,
     GOOG_NOISE_SUPPRESSION},
    {&AudioProcessingProperties::goog_experimental_noise_suppression,
     GOOG_EXPERIMENTAL_NOISE_SUPPRESSION},
    {&AudioProcessingProperties::goog_highpass_filter, GOOG_HIGHPASS_FILTER},
    {&AudioProcessingProperties::goog_experimental_auto_gain_control,
     GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL}};

// Selects the best value from the nonempty |set|, subject to |constraint| and
// determines the associated fitness. The first selection criteria is equality
// to |constraint.Ideal()|, followed by equality to |default_value|. There is
// always a single best value.
std::tuple<bool, double> SelectBoolAndFitness(
    const DiscreteSet<bool>& set,
    const blink::BooleanConstraint& constraint,
    bool default_value) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal()))
    return std::make_tuple(constraint.Ideal(), 1.0);

  if (set.is_universal())
    return std::make_tuple(default_value, 0.0);

  DCHECK_EQ(set.elements().size(), 1U);
  return std::make_tuple(set.FirstElement(), 0.0);
}

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

// Selects the best value from the nonempty |set|, subject to |constraint| and
// determines the associated fitness. The first selection criteria is inclusion
// in |constraint.Ideal()|, followed by equality to |default_value|. There is
// always a single best value.
std::tuple<std::string, double> SelectStringAndFitness(
    const DiscreteSet<std::string>& set,
    const blink::StringConstraint& constraint,
    const std::string& default_value) {
  DCHECK(!set.IsEmpty());

  if (constraint.HasIdeal()) {
    for (const blink::WebString& ideal_candidate : constraint.Ideal()) {
      std::string candidate = ideal_candidate.Utf8();
      if (set.Contains(candidate))
        return std::make_tuple(candidate, 1.0);
    }
  }

  if (set.Contains(default_value))
    return std::make_tuple(default_value, 0.0);

  return std::make_tuple(set.FirstElement(), 0.0);
}

bool SelectUseEchoCancellation(base::Optional<bool> echo_cancellation,
                               base::Optional<bool> goog_echo_cancellation,
                               bool is_device_capture) {
  DCHECK(echo_cancellation && goog_echo_cancellation
             ? *echo_cancellation == *goog_echo_cancellation
             : true);
  if (echo_cancellation)
    return *echo_cancellation;
  if (goog_echo_cancellation)
    return *goog_echo_cancellation;

  // Echo cancellation is enabled by default for device capture and disabled by
  // default for content capture.
  return is_device_capture;
}

std::vector<std::string> GetEchoCancellationTypesFromParameters(
    const media::AudioParameters& audio_parameters) {
  if (audio_parameters.effects() &
      (media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER |
       media::AudioParameters::ECHO_CANCELLER)) {
    // If the system/hardware supports echo cancellation, return all echo
    // cancellers.
    return {blink::kEchoCancellationTypeBrowser,
            blink::kEchoCancellationTypeAec3,
            blink::kEchoCancellationTypeSystem};
  }

  // The browser and AEC3 echo cancellers are always available.
  return {blink::kEchoCancellationTypeBrowser,
          blink::kEchoCancellationTypeAec3};
}

// This class represents all the candidates settings for a single audio device.
class SingleDeviceCandidateSet {
 public:
  explicit SingleDeviceCandidateSet(
      const AudioDeviceCaptureCapability& capability)
      : parameters_(capability.Parameters()) {
    // If empty, all values for the deviceId constraint are allowed and
    // |device_id_set_| is the universal set. Otherwise, limit |device_id_set_|
    // to the known device ID.
    if (!capability.DeviceID().empty())
      device_id_set_ = DiscreteSet<std::string>({capability.DeviceID()});

    if (!capability.GroupID().empty())
      group_id_set_ = DiscreteSet<std::string>({capability.GroupID()});

    MediaStreamAudioSource* source = capability.source();

    // Set up echo cancellation types. Depending on if we have a source or not
    // it's set up differently.
    if (!source) {
      echo_cancellation_type_set_ = DiscreteSet<std::string>(
          GetEchoCancellationTypesFromParameters(parameters_));
      return;
    }

    // Properties not related to audio processing.
    bool_sets_[HOTWORD_ENABLED] =
        DiscreteSet<bool>({source->hotword_enabled()});
    bool_sets_[DISABLE_LOCAL_ECHO] =
        DiscreteSet<bool>({source->disable_local_echo()});
    bool_sets_[RENDER_TO_ASSOCIATED_SINK] =
        DiscreteSet<bool>({source->RenderToAssociatedSinkEnabled()});

    // Properties related with audio processing.
    AudioProcessingProperties properties;
    ProcessedLocalAudioSource* processed_source =
        ProcessedLocalAudioSource::From(source);
    if (processed_source)
      properties = processed_source->audio_processing_properties();
    else
      properties.DisableDefaultProperties();

    const bool system_echo_cancellation_available =
        (properties.echo_cancellation_type ==
             EchoCancellationType::kEchoCancellationSystem ||
         !processed_source) &&
        parameters_.effects() & media::AudioParameters::ECHO_CANCELLER;

    const bool experimental_system_cancellation_available =
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationSystem &&
        parameters_.effects() &
            media::AudioParameters::EXPERIMENTAL_ECHO_CANCELLER;

    const bool system_echo_cancellation_enabled =
        system_echo_cancellation_available ||
        experimental_system_cancellation_available;

    const bool echo_cancellation_enabled =
        properties.EchoCancellationIsWebRtcProvided() ||
        system_echo_cancellation_enabled;

    bool_sets_[ECHO_CANCELLATION] =
        DiscreteSet<bool>({echo_cancellation_enabled});
    bool_sets_[GOOG_ECHO_CANCELLATION] = bool_sets_[ECHO_CANCELLATION];

    if (properties.echo_cancellation_type ==
        EchoCancellationType::kEchoCancellationAec2) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeBrowser});
    } else if (properties.echo_cancellation_type ==
               EchoCancellationType::kEchoCancellationAec3) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeAec3});
    } else if (system_echo_cancellation_enabled) {
      echo_cancellation_type_set_ =
          DiscreteSet<std::string>({blink::kEchoCancellationTypeSystem});
    }

    bool_sets_[GOOG_AUDIO_MIRRORING] =
        DiscreteSet<bool>({properties.goog_audio_mirroring});

    for (auto& entry : kAudioPropertyConstraintMap) {
      bool_sets_[entry.bool_set_index] =
          DiscreteSet<bool>({properties.*entry.audio_property});
    }

#if DCHECK_IS_ON()
    for (const auto& bool_set : bool_sets_) {
      DCHECK(!bool_set.is_universal());
      DCHECK(!bool_set.IsEmpty());
    }
#endif
  }

  // Accessors
  const char* failed_constraint_name() const { return failed_constraint_name_; }
  const DiscreteSet<std::string>& device_id_set() const {
    return device_id_set_;
  }

  bool IsEmpty() const { return failed_constraint_name_ != nullptr; }

  void ApplyConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set) {
    device_id_set_ = device_id_set_.Intersection(
        media_constraints::StringSetFromConstraint(constraint_set.device_id));
    if (device_id_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.device_id.GetName();
      return;
    }

    group_id_set_ = group_id_set_.Intersection(
        media_constraints::StringSetFromConstraint(constraint_set.group_id));
    if (group_id_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.group_id.GetName();
      return;
    }

    for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
      bool_sets_[i] =
          bool_sets_[i].Intersection(media_constraints::BoolSetFromConstraint(
              constraint_set.*kBlinkBoolConstraintFields[i]));
      if (bool_sets_[i].IsEmpty()) {
        failed_constraint_name_ =
            (constraint_set.*kBlinkBoolConstraintFields[i]).GetName();
        return;
      }
    }

    // echoCancellation and googEchoCancellation constraints should not
    // contradict each other. Mark the set as empty if they do.
    DiscreteSet<bool> echo_cancellation_intersection =
        bool_sets_[ECHO_CANCELLATION].Intersection(
            bool_sets_[GOOG_ECHO_CANCELLATION]);
    if (echo_cancellation_intersection.IsEmpty()) {
      failed_constraint_name_ =
          blink::WebMediaTrackConstraintSet().echo_cancellation.GetName();
      return;
    }

    echo_cancellation_type_set_ = echo_cancellation_type_set_.Intersection(
        media_constraints::StringSetFromConstraint(
            constraint_set.echo_cancellation_type));
    if (echo_cancellation_type_set_.IsEmpty()) {
      failed_constraint_name_ = constraint_set.echo_cancellation_type.GetName();
      return;
    }

    // If echo cancellation constraint is not true, the type set should not have
    // explicit elements.
    if (!bool_sets_[ECHO_CANCELLATION].Contains(true) &&
        constraint_set.echo_cancellation_type.HasExact()) {
      failed_constraint_name_ = constraint_set.echo_cancellation_type.GetName();
      return;
    }
  }

  // Returns the settings supported by this SingleDeviceCandidateSet that best
  // satisfy the ideal values in |basic_constraint_set| as well as the
  // associated fitness, which is based on
  // https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
  std::tuple<AudioCaptureSettings, double> SelectBestSettingsAndFitness(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const std::string& default_device_id,
      const std::string& media_stream_source,
      bool should_disable_hardware_noise_suppression) const {
    double fitness = 0.0;
    double sub_fitness;

    // Fitness and setting for the device ID.
    std::string device_id;
    std::tie(device_id, sub_fitness) = SelectStringAndFitness(
        device_id_set_, constraint_set.device_id, default_device_id);
    fitness += sub_fitness;

    // Fitness for the group ID.
    std::tie(std::ignore, sub_fitness) =
        SelectStringAndFitness(group_id_set_, constraint_set.group_id, "");
    fitness += sub_fitness;

    // googHotword, disableLocalEcho, and chromeRenderToAssociatedSink cannot
    // have ideal values, hence we explicitely ignore the fitness value for all
    // three.
    bool hotword_enabled;
    std::tie(hotword_enabled, std::ignore) = SelectBoolAndFitness(
        bool_sets_[HOTWORD_ENABLED], constraint_set.hotword_enabled, false);

    bool disable_local_echo;
    std::tie(disable_local_echo, std::ignore) = SelectBoolAndFitness(
        bool_sets_[DISABLE_LOCAL_ECHO], constraint_set.disable_local_echo,
        media_stream_source != kMediaStreamSourceDesktop);

    bool render_to_associated_sink;
    std::tie(render_to_associated_sink, std::ignore) =
        SelectBoolAndFitness(bool_sets_[RENDER_TO_ASSOCIATED_SINK],
                             constraint_set.render_to_associated_sink, false);

    // Fitness and settings for the properties associated to audio processing.
    AudioProcessingProperties audio_processing_properties;
    std::tie(audio_processing_properties, sub_fitness) =
        SelectAudioProcessingPropertiesAndFitness(
            constraint_set, media_stream_source.empty(),
            should_disable_hardware_noise_suppression);
    fitness += sub_fitness;

    auto settings = AudioCaptureSettings(
        std::move(device_id), hotword_enabled, disable_local_echo,
        render_to_associated_sink, audio_processing_properties);
    return std::make_tuple(settings, fitness);
  }

 private:
  std::tuple<EchoCancellationType, double> SelectEchoCancellationTypeAndFitness(
      const blink::StringConstraint& echo_cancellation_type,
      const media::AudioParameters& audio_parameters) const {
    double fitness = 0.0;

    // Try to use an ideal candidate, if supplied.
    base::Optional<std::string> selected_type;
    if (echo_cancellation_type.HasIdeal()) {
      for (const auto& ideal : echo_cancellation_type.Ideal()) {
        std::string candidate = ideal.Utf8();
        if (echo_cancellation_type_set_.Contains(candidate)) {
          selected_type = candidate;
          fitness = 1.0;
          break;
        }
      }
    }

    // If no ideal, or none that worked, and the set contains only one value,
    // pick that.
    if (!selected_type) {
      if (!echo_cancellation_type_set_.is_universal() &&
          echo_cancellation_type_set_.elements().size() == 1) {
        selected_type = echo_cancellation_type_set_.FirstElement();
      }
    }

    // Return type based the selected type.
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
    if (audio_parameters.IsValid() &&
        audio_parameters.effects() & media::AudioParameters::ECHO_CANCELLER) {
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

  // Returns the audio-processing properties supported by this
  // SingleDeviceCandidateSet that best satisfy the ideal values in
  // |basic_constraint_set| and the associated fitness.
  std::tuple<AudioProcessingProperties, double>
  SelectAudioProcessingPropertiesAndFitness(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      bool is_device_capture,
      bool should_disable_hardware_noise_suppression) const {
    DCHECK(!IsEmpty());

    base::Optional<bool> echo_cancellation = SelectOptionalBool(
        bool_sets_[ECHO_CANCELLATION], constraint_set.echo_cancellation);
    // Audio-processing properties are disabled by default for content capture,
    // or if the |echo_cancellation| constraint is false.
    bool default_audio_processing_value = true;
    if (!is_device_capture || (echo_cancellation && !*echo_cancellation))
      default_audio_processing_value = false;

    base::Optional<bool> goog_echo_cancellation =
        SelectOptionalBool(bool_sets_[GOOG_ECHO_CANCELLATION],
                           constraint_set.goog_echo_cancellation);

    const bool use_echo_cancellation = SelectUseEchoCancellation(
        echo_cancellation, goog_echo_cancellation, is_device_capture);

    AudioProcessingProperties properties;
    bool fitness = 0;
    bool sub_fitness;

    if (use_echo_cancellation) {
      std::tie(properties.echo_cancellation_type, sub_fitness) =
          SelectEchoCancellationTypeAndFitness(
              constraint_set.echo_cancellation_type, parameters_);
      fitness += sub_fitness;
    } else {
      properties.echo_cancellation_type =
          EchoCancellationType::kEchoCancellationDisabled;
    }

    properties.disable_hw_noise_suppression =
        should_disable_hardware_noise_suppression &&
        properties.echo_cancellation_type ==
            EchoCancellationType::kEchoCancellationSystem;

    std::tie(properties.goog_audio_mirroring, sub_fitness) =
        SelectBoolAndFitness(bool_sets_[GOOG_AUDIO_MIRRORING],
                             constraint_set.goog_audio_mirroring,
                             properties.goog_audio_mirroring);
    fitness += sub_fitness;

    for (auto& entry : kAudioPropertyConstraintMap) {
      std::tie(properties.*entry.audio_property, sub_fitness) =
          SelectBoolAndFitness(
              bool_sets_[entry.bool_set_index],
              constraint_set.*kBlinkBoolConstraintFields[entry.bool_set_index],
              default_audio_processing_value &&
                  properties.*entry.audio_property);
      fitness += sub_fitness;
    }

    return std::make_tuple(properties, fitness);
  }

  static constexpr blink::BooleanConstraint
      blink::WebMediaTrackConstraintSet::* const
          kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS] = {
              &blink::WebMediaTrackConstraintSet::hotword_enabled,
              &blink::WebMediaTrackConstraintSet::disable_local_echo,
              &blink::WebMediaTrackConstraintSet::render_to_associated_sink,
              &blink::WebMediaTrackConstraintSet::echo_cancellation,
              &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
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
                  goog_experimental_auto_gain_control};

  const char* failed_constraint_name_ = nullptr;
  DiscreteSet<std::string> device_id_set_;
  DiscreteSet<std::string> group_id_set_;
  std::array<DiscreteSet<bool>, NUM_BOOL_CONSTRAINTS> bool_sets_;
  DiscreteSet<std::string> echo_cancellation_type_set_;
  media::AudioParameters parameters_;
};

constexpr blink::BooleanConstraint blink::WebMediaTrackConstraintSet::* const
    SingleDeviceCandidateSet::kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS];

// This class represents a set of possible candidate settings.
// The SelectSettings algorithm starts with a set containing all possible
// candidates based on system/hardware capabilities and/or allowed values for
// supported properties. The set is then reduced progressively as the basic and
// advanced constraint sets are applied. In the end, if the set of candidates is
// empty, SelectSettings fails. If not, the ideal values (if any) or tie breaker
// rules are used to select the final settings based on the candidates that
// survived the application of the constraint sets. This class is implemented as
// a collection of more specific sets for the various supported properties. If
// any of the specific sets is empty, the whole AudioCaptureCandidates set is
// considered empty as well.
class AudioCaptureCandidates {
 public:
  AudioCaptureCandidates(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const AudioDeviceCaptureCapabilities& capabilities) {
    for (const auto& capability : capabilities)
      candidate_sets_.emplace_back(capability);

    ApplyConstraintSet(constraint_set);
  }

  const char* failed_constraint_name() const { return failed_constraint_name_; }
  bool IsEmpty() const { return failed_constraint_name_ != nullptr; }

  void ApplyConstraintSet(
      const blink::WebMediaTrackConstraintSet& constraint_set) {
    for (auto& candidate_set : candidate_sets_)
      candidate_set.ApplyConstraintSet(constraint_set);

    const char* failed_constraint_name = nullptr;
    for (auto it = candidate_sets_.begin(); it != candidate_sets_.end();) {
      if (it->IsEmpty()) {
        DCHECK(it->failed_constraint_name());
        failed_constraint_name = it->failed_constraint_name();
        it = candidate_sets_.erase(it);
      } else {
        ++it;
      }
    }

    if (candidate_sets_.empty())
      failed_constraint_name_ = failed_constraint_name;
  }

  // Returns the settings that best satisfy the ideal values in
  // |basic_constraint_set| subject to the limitations of this
  // AudioCaptureCandidates object.
  AudioCaptureSettings SelectBestSettings(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const std::string& default_device_id,
      const std::string& media_stream_source,
      bool should_disable_hardware_noise_suppression) const {
    DCHECK(!candidate_sets_.empty());

    auto best_candidate = candidate_sets_.end();
    std::vector<double> best_score({-1, -1});
    AudioCaptureSettings best_settings;

    for (auto it = candidate_sets_.begin(); it != candidate_sets_.end(); ++it) {
      DCHECK(!it->IsEmpty());

      std::vector<double> score(2);
      AudioCaptureSettings settings;

      std::tie(settings, score[0]) = it->SelectBestSettingsAndFitness(
          constraint_set, default_device_id, media_stream_source,
          should_disable_hardware_noise_suppression);
      score[1] = it->device_id_set().Contains(default_device_id) ? 1.0 : 0.0;

      if (score > best_score) {
        best_score = score;
        best_candidate = it;
        best_settings = settings;
      }
    }

    DCHECK(best_candidate != candidate_sets_.end());
    DCHECK(!best_candidate->IsEmpty());
    return best_settings;
  }

 private:
  const char* failed_constraint_name_ = nullptr;
  std::vector<SingleDeviceCandidateSet> candidate_sets_;
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
  std::string media_stream_source = GetMediaStreamSource(constraints);
  bool is_device_capture = media_stream_source.empty();
  if (capabilities.empty())
    return AudioCaptureSettings();

  AudioCaptureCandidates candidates(constraints.Basic(), capabilities);
  if (candidates.IsEmpty())
    return AudioCaptureSettings(candidates.failed_constraint_name());

  for (const auto& advanced_set : constraints.Advanced()) {
    AudioCaptureCandidates copy = candidates;
    candidates.ApplyConstraintSet(advanced_set);
    if (candidates.IsEmpty())
      candidates = std::move(copy);
  }
  DCHECK(!candidates.IsEmpty());

  std::string default_device_id;
  if (is_device_capture)
    default_device_id = capabilities.begin()->DeviceID();

  return candidates.SelectBestSettings(
      constraints.Basic(), default_device_id, media_stream_source,
      should_disable_hardware_noise_suppression);
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
