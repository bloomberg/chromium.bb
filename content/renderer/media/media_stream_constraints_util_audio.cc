// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_constraints_util_audio.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/media_stream_constraints_util_sets.h"
#include "content/renderer/media/media_stream_video_source.h"
#include "media/base/limits.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

namespace {

// This class has the same data as ::mojom::AudioInputDeviceCapabilities, but
// adds extra operations to simplify the device-selection code.
class AudioDeviceInfo {
 public:
  // This constructor is intended for device capture.
  explicit AudioDeviceInfo(
      const ::mojom::AudioInputDeviceCapabilitiesPtr& device_info)
      : device_id_(device_info->device_id),
        parameters_(device_info->parameters) {
    DCHECK(parameters_.IsValid());
  }

  // This constructor is intended for content capture, where constraints on
  // audio parameters are not supported.
  explicit AudioDeviceInfo(std::string device_id)
      : device_id_(std::move(device_id)) {}

  bool operator==(const AudioDeviceInfo& other) const {
    return device_id_ == other.device_id_;
  }

  // Accessors
  const std::string& device_id() const { return device_id_; }
  const media::AudioParameters& parameters() const { return parameters_; }

  // Convenience accessors
  int SampleRate() const {
    DCHECK(parameters_.IsValid());
    return parameters_.sample_rate();
  }
  int SampleSize() const {
    DCHECK(parameters_.IsValid());
    return parameters_.bits_per_sample();
  }
  int ChannelCount() const {
    DCHECK(parameters_.IsValid());
    return parameters_.channels();
  }
  int Effects() const {
    DCHECK(parameters_.IsValid());
    return parameters_.effects();
  }

 private:
  std::string device_id_;
  media::AudioParameters parameters_;
};

using AudioDeviceSet = DiscreteSet<AudioDeviceInfo>;

AudioDeviceSet AudioDeviceSetForDeviceCapture(
    const blink::WebMediaTrackConstraintSet& constraint_set,
    const AudioDeviceCaptureCapabilities& capabilities,
    const char** failed_constraint_name) {
  std::vector<AudioDeviceInfo> result;
  *failed_constraint_name = "";
  for (auto& device_capabilities : capabilities) {
    if (!constraint_set.device_id.Matches(
            blink::WebString::FromASCII(device_capabilities->device_id))) {
      if (failed_constraint_name)
        *failed_constraint_name = constraint_set.device_id.GetName();
      continue;
    }
    *failed_constraint_name =
        IsOutsideConstraintRange(constraint_set.sample_rate,
                                 device_capabilities->parameters.sample_rate());
    if (*failed_constraint_name)
      continue;

    *failed_constraint_name = IsOutsideConstraintRange(
        constraint_set.sample_size,
        device_capabilities->parameters.bits_per_sample());
    if (*failed_constraint_name)
      continue;

    *failed_constraint_name =
        IsOutsideConstraintRange(constraint_set.channel_count,
                                 device_capabilities->parameters.channels());
    if (*failed_constraint_name)
      continue;

    result.push_back(AudioDeviceInfo(device_capabilities));
  }

  if (!result.empty())
    *failed_constraint_name = nullptr;

  return AudioDeviceSet(result);
}

AudioDeviceSet AudioDeviceSetForContentCapture(
    const blink::WebMediaTrackConstraintSet& constraint_set,
    const char** failed_constraint_name = nullptr) {
  if (!constraint_set.device_id.HasExact())
    return AudioDeviceSet::UniversalSet();

  std::vector<AudioDeviceInfo> result;
  for (auto& device_id : constraint_set.device_id.Exact())
    result.push_back(AudioDeviceInfo(device_id.Utf8()));

  return AudioDeviceSet(result);
}

// This class represents a set of possible candidate settings.
// The SelectSettings algorithm starts with a set containing all possible
// candidates based on hardware capabilities and/or allowed values for supported
// properties. The is then reduced progressively as the basic and advanced
// constraint sets are applied.
// In the end, if the set of candidates is empty, SelectSettings fails.
// If not, the ideal values (if any) or tie breaker rules are used to select
// the final settings based on the candidates that survived the application
// of the constraint sets.
// This class is implemented as a collection of more specific sets for the
// various supported properties. If any of the specific sets is empty, the
// whole AudioCaptureCandidates set is considered empty as well.
class AudioCaptureCandidates {
 public:
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
    GOOG_BEAMFORMING,
    GOOG_HIGHPASS_FILTER,
    GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL,
    NUM_BOOL_CONSTRAINTS
  };

  AudioCaptureCandidates();

  AudioCaptureCandidates(
      const blink::WebMediaTrackConstraintSet& constraint_set,
      const AudioDeviceCaptureCapabilities& capabilities,
      bool is_device_capture);

  // Set operations.
  bool IsEmpty() const { return failed_constraint_name_ != nullptr; }
  AudioCaptureCandidates Intersection(const AudioCaptureCandidates& other);

  // Accessors.
  const char* failed_constraint_name() const { return failed_constraint_name_; }
  const AudioDeviceSet& audio_device_set() const { return audio_device_set_; }
  const DiscreteSet<std::string>& goog_array_geometry_set() const {
    return goog_array_geometry_set_;
  }

  // Accessor for boolean sets.
  const DiscreteSet<bool>& GetBoolSet(BoolConstraint property) const {
    DCHECK_GE(property, 0);
    DCHECK_LT(property, NUM_BOOL_CONSTRAINTS);
    return bool_sets_[property];
  }

  // Convenience accessors.
  const DiscreteSet<bool>& hotword_enabled_set() const {
    return bool_sets_[HOTWORD_ENABLED];
  }
  const DiscreteSet<bool>& disable_local_echo_set() const {
    return bool_sets_[DISABLE_LOCAL_ECHO];
  }
  const DiscreteSet<bool>& render_to_associated_sink_set() const {
    return bool_sets_[RENDER_TO_ASSOCIATED_SINK];
  }
  const DiscreteSet<bool>& echo_cancellation_set() const {
    return bool_sets_[ECHO_CANCELLATION];
  }
  const DiscreteSet<bool>& goog_echo_cancellation_set() const {
    return bool_sets_[GOOG_ECHO_CANCELLATION];
  }
  const DiscreteSet<bool>& goog_audio_mirroring_set() const {
    return bool_sets_[GOOG_AUDIO_MIRRORING];
  }

 private:
  void MaybeUpdateFailedNonDeviceConstraintName();
  void CheckContradictoryEchoCancellation();

  // Maps BoolConstraint values to fields in blink::WebMediaTrackConstraintSet.
  static const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*
      kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS];

  const char* failed_constraint_name_;

  AudioDeviceSet audio_device_set_;  // Device-related constraints.
  std::array<DiscreteSet<bool>, NUM_BOOL_CONSTRAINTS> bool_sets_;
  DiscreteSet<std::string> goog_array_geometry_set_;
};

const blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*
    AudioCaptureCandidates::kBlinkBoolConstraintFields[NUM_BOOL_CONSTRAINTS] = {
        &blink::WebMediaTrackConstraintSet::hotword_enabled,
        &blink::WebMediaTrackConstraintSet::disable_local_echo,
        &blink::WebMediaTrackConstraintSet::render_to_associated_sink,
        &blink::WebMediaTrackConstraintSet::echo_cancellation,
        &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
        &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
        &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
        &blink::WebMediaTrackConstraintSet::goog_experimental_echo_cancellation,
        &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
        &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
        &blink::WebMediaTrackConstraintSet::goog_experimental_noise_suppression,
        &blink::WebMediaTrackConstraintSet::goog_beamforming,
        &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
        &blink::WebMediaTrackConstraintSet::
            goog_experimental_auto_gain_control};

// directly mapped boolean sets to audio properties
struct BoolSetPropertyEntry {
  DiscreteSet<bool> AudioCaptureCandidates::*bool_set;
  bool AudioProcessingProperties::*bool_field;
  bool default_value;
};

AudioCaptureCandidates::AudioCaptureCandidates()
    : failed_constraint_name_(nullptr) {}

AudioCaptureCandidates::AudioCaptureCandidates(
    const blink::WebMediaTrackConstraintSet& constraint_set,
    const AudioDeviceCaptureCapabilities& capabilities,
    bool is_device_capture)
    : failed_constraint_name_(nullptr),
      audio_device_set_(
          is_device_capture
              ? AudioDeviceSetForDeviceCapture(constraint_set,
                                               capabilities,
                                               &failed_constraint_name_)
              : AudioDeviceSetForContentCapture(constraint_set,
                                                &failed_constraint_name_)),
      goog_array_geometry_set_(
          StringSetFromConstraint(constraint_set.goog_array_geometry)) {
  for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
    bool_sets_[i] =
        BoolSetFromConstraint(constraint_set.*kBlinkBoolConstraintFields[i]);
  }
  MaybeUpdateFailedNonDeviceConstraintName();
}

AudioCaptureCandidates AudioCaptureCandidates::Intersection(
    const AudioCaptureCandidates& other) {
  AudioCaptureCandidates intersection;
  intersection.audio_device_set_ =
      audio_device_set_.Intersection(other.audio_device_set_);
  if (intersection.audio_device_set_.IsEmpty()) {
    // To mark the intersection as empty, it is necessary to assign a
    // a non-null value to |failed_constraint_name_|. The specific value
    // for an intersection does not actually matter, since the intersection
    // is discarded if empty.
    intersection.failed_constraint_name_ = "some device constraint";
    return intersection;
  }
  for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
    intersection.bool_sets_[i] =
        bool_sets_[i].Intersection(other.bool_sets_[i]);
  }
  intersection.goog_array_geometry_set_ =
      goog_array_geometry_set_.Intersection(other.goog_array_geometry_set_);
  intersection.MaybeUpdateFailedNonDeviceConstraintName();

  return intersection;
}

void AudioCaptureCandidates::MaybeUpdateFailedNonDeviceConstraintName() {
  blink::WebMediaTrackConstraintSet constraint_set;
  for (size_t i = 0; i < NUM_BOOL_CONSTRAINTS; ++i) {
    if (bool_sets_[i].IsEmpty()) {
      failed_constraint_name_ =
          (constraint_set.*kBlinkBoolConstraintFields[i]).GetName();
    }
  }
  if (goog_array_geometry_set_.IsEmpty())
    failed_constraint_name_ = constraint_set.goog_array_geometry.GetName();

  CheckContradictoryEchoCancellation();
}

void AudioCaptureCandidates::CheckContradictoryEchoCancellation() {
  DiscreteSet<bool> echo_cancellation_intersection =
      bool_sets_[ECHO_CANCELLATION].Intersection(
          bool_sets_[GOOG_ECHO_CANCELLATION]);
  // echoCancellation and googEchoCancellation constraints should not
  // contradict each other. Mark the set as empty if they do.
  if (echo_cancellation_intersection.IsEmpty()) {
    failed_constraint_name_ =
        blink::WebMediaTrackConstraintSet().echo_cancellation.GetName();
  }
}

// Fitness function for constraints involved in device selection.
//  Based on https://w3c.github.io/mediacapture-main/#dfn-fitness-distance
double DeviceInfoFitness(
    bool is_device_capture,
    const AudioDeviceInfo& device_info,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set) {
  double fitness = 0.0;
  fitness += StringConstraintFitnessDistance(
      blink::WebString::FromASCII(device_info.device_id()),
      basic_constraint_set.device_id);

  if (!is_device_capture)
    return fitness;

  if (basic_constraint_set.sample_rate.HasIdeal()) {
    fitness += NumericConstraintFitnessDistance(
        device_info.SampleRate(), basic_constraint_set.sample_rate.Ideal());
  }

  if (basic_constraint_set.sample_size.HasIdeal()) {
    fitness += NumericConstraintFitnessDistance(
        device_info.SampleSize(), basic_constraint_set.sample_size.Ideal());
  }

  if (basic_constraint_set.channel_count.HasIdeal()) {
    fitness += NumericConstraintFitnessDistance(
        device_info.ChannelCount(), basic_constraint_set.channel_count.Ideal());
  }

  return fitness;
}

AudioDeviceInfo SelectDevice(
    const AudioDeviceSet& audio_device_set,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const std::string& default_device_id,
    bool is_device_capture) {
  DCHECK(!audio_device_set.IsEmpty());
  if (audio_device_set.is_universal()) {
    DCHECK(!is_device_capture);
    std::string device_id;
    if (basic_constraint_set.device_id.HasIdeal()) {
      device_id = basic_constraint_set.device_id.Ideal().begin()->Utf8();
    }
    return AudioDeviceInfo(std::move(device_id));
  }

  std::vector<double> best_fitness({HUGE_VAL, HUGE_VAL});
  auto best_candidate = audio_device_set.elements().end();
  for (auto it = audio_device_set.elements().begin();
       it != audio_device_set.elements().end(); ++it) {
    std::vector<double> fitness;
    // First criterion is spec-based fitness function. Second criterion is
    // being the default device.
    fitness.push_back(
        DeviceInfoFitness(is_device_capture, *it, basic_constraint_set));
    fitness.push_back(it->device_id() == default_device_id ? 0.0 : HUGE_VAL);
    if (fitness < best_fitness) {
      best_fitness = std::move(fitness);
      best_candidate = it;
    }
  }
  DCHECK(best_candidate != audio_device_set.elements().end());
  return *best_candidate;
}

bool SelectBool(const DiscreteSet<bool>& set,
                const blink::BooleanConstraint& constraint,
                bool default_value) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal())) {
    return constraint.Ideal();
  }

  // Return default value if unconstrained.
  if (set.is_universal()) {
    return default_value;
  }
  DCHECK_EQ(set.elements().size(), 1U);
  return set.FirstElement();
}

base::Optional<bool> SelectOptionalBool(
    const DiscreteSet<bool>& set,
    const blink::BooleanConstraint& constraint) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal() && set.Contains(constraint.Ideal())) {
    return constraint.Ideal();
  }

  // Return no value if unconstrained.
  if (set.is_universal()) {
    return base::Optional<bool>();
  }
  DCHECK_EQ(set.elements().size(), 1U);
  return set.FirstElement();
}

base::Optional<std::string> SelectOptionalString(
    const DiscreteSet<std::string>& set,
    const blink::StringConstraint& constraint) {
  DCHECK(!set.IsEmpty());
  if (constraint.HasIdeal()) {
    for (const auto& ideal_candidate : constraint.Ideal()) {
      std::string candidate = ideal_candidate.Utf8();
      if (set.Contains(candidate)) {
        return candidate;
      }
    }
  }

  // Return no value if unconstrained.
  if (set.is_universal()) {
    return base::Optional<std::string>();
  }
  return set.FirstElement();
}

bool SelectEnableSwEchoCancellation(
    base::Optional<bool> echo_cancellation,
    base::Optional<bool> goog_echo_cancellation,
    const media::AudioParameters& audio_parameters,
    bool default_audio_processing_value) {
  // If there is hardware echo cancellation, return false.
  if (audio_parameters.IsValid() &&
      (audio_parameters.effects() & media::AudioParameters::ECHO_CANCELLER))
    return false;
  DCHECK(echo_cancellation && goog_echo_cancellation
             ? *echo_cancellation == *goog_echo_cancellation
             : true);
  if (echo_cancellation)
    return *echo_cancellation;
  if (goog_echo_cancellation)
    return *goog_echo_cancellation;

  return default_audio_processing_value;
}

struct AudioPropertyConstraintPair {
  bool AudioProcessingProperties::*audio_property;
  AudioCaptureCandidates::BoolConstraint bool_set_index;
  blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*constraint;
};

// Boolean audio properties that are mapped directly to a boolean constraint
// and which are subject to the same processing.
const AudioPropertyConstraintPair kAudioPropertyConstraintMap[] = {
    {&AudioProcessingProperties::goog_auto_gain_control,
     AudioCaptureCandidates::GOOG_AUTO_GAIN_CONTROL,
     &blink::WebMediaTrackConstraintSet::goog_auto_gain_control},
    {&AudioProcessingProperties::goog_experimental_echo_cancellation,
     AudioCaptureCandidates::GOOG_EXPERIMENTAL_ECHO_CANCELLATION,
     &blink::WebMediaTrackConstraintSet::goog_experimental_echo_cancellation},
    {&AudioProcessingProperties::goog_typing_noise_detection,
     AudioCaptureCandidates::GOOG_TYPING_NOISE_DETECTION,
     &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection},
    {&AudioProcessingProperties::goog_noise_suppression,
     AudioCaptureCandidates::GOOG_NOISE_SUPPRESSION,
     &blink::WebMediaTrackConstraintSet::goog_noise_suppression},
    {&AudioProcessingProperties::goog_experimental_noise_suppression,
     AudioCaptureCandidates::GOOG_EXPERIMENTAL_NOISE_SUPPRESSION,
     &blink::WebMediaTrackConstraintSet::goog_experimental_noise_suppression},
    {&AudioProcessingProperties::goog_beamforming,
     AudioCaptureCandidates::GOOG_BEAMFORMING,
     &blink::WebMediaTrackConstraintSet::goog_beamforming},
    {&AudioProcessingProperties::goog_highpass_filter,
     AudioCaptureCandidates::GOOG_HIGHPASS_FILTER,
     &blink::WebMediaTrackConstraintSet::goog_highpass_filter},
    {&AudioProcessingProperties::goog_experimental_auto_gain_control,
     AudioCaptureCandidates::GOOG_EXPERIMENTAL_AUTO_GAIN_CONTROL,
     &blink::WebMediaTrackConstraintSet::goog_experimental_auto_gain_control}};

AudioProcessingProperties SelectAudioProcessingProperties(
    const AudioCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const media::AudioParameters& audio_parameters,
    bool is_device_capture) {
  DCHECK(!candidates.IsEmpty());
  base::Optional<bool> echo_cancellation =
      SelectOptionalBool(candidates.echo_cancellation_set(),
                         basic_constraint_set.echo_cancellation);
  // Audio-processing properties are disabled by default for content capture, or
  // if the |echo_cancellation| constraint is false.
  bool default_audio_processing_value = true;
  if (!is_device_capture || (echo_cancellation && !*echo_cancellation))
    default_audio_processing_value = false;

  base::Optional<bool> goog_echo_cancellation =
      SelectOptionalBool(candidates.goog_echo_cancellation_set(),
                         basic_constraint_set.goog_echo_cancellation);

  AudioProcessingProperties properties;
  properties.enable_sw_echo_cancellation = SelectEnableSwEchoCancellation(
      echo_cancellation, goog_echo_cancellation, audio_parameters,
      default_audio_processing_value);
  properties.disable_hw_echo_cancellation =
      (echo_cancellation && !*echo_cancellation) ||
      (goog_echo_cancellation && !*goog_echo_cancellation);

  properties.goog_audio_mirroring =
      SelectBool(candidates.goog_audio_mirroring_set(),
                 basic_constraint_set.goog_audio_mirroring,
                 properties.goog_audio_mirroring);

  for (auto& entry : kAudioPropertyConstraintMap) {
    properties.*entry.audio_property = SelectBool(
        candidates.GetBoolSet(entry.bool_set_index),
        basic_constraint_set.*entry.constraint,
        default_audio_processing_value && properties.*entry.audio_property);
  }

  base::Optional<std::string> array_geometry =
      SelectOptionalString(candidates.goog_array_geometry_set(),
                           basic_constraint_set.goog_array_geometry);
  std::vector<media::Point> parsed_positions;
  if (array_geometry)
    parsed_positions = media::ParsePointsFromString(*array_geometry);
  bool are_valid_parsed_positions =
      !parsed_positions.empty() || (array_geometry && array_geometry->empty());
  properties.goog_array_geometry = are_valid_parsed_positions
                                       ? std::move(parsed_positions)
                                       : audio_parameters.mic_positions();

  return properties;
}

AudioCaptureSettings SelectResult(
    const AudioCaptureCandidates& candidates,
    const blink::WebMediaTrackConstraintSet& basic_constraint_set,
    const std::string& default_device_id,
    const std::string& media_stream_source) {
  bool is_device_capture = media_stream_source.empty();
  AudioDeviceInfo device_info =
      SelectDevice(candidates.audio_device_set(), basic_constraint_set,
                   default_device_id, is_device_capture);
  bool hotword_enabled =
      SelectBool(candidates.hotword_enabled_set(),
                 basic_constraint_set.hotword_enabled, false);
  bool disable_local_echo_default =
      media_stream_source != kMediaStreamSourceDesktop;
  bool disable_local_echo = SelectBool(candidates.disable_local_echo_set(),
                                       basic_constraint_set.disable_local_echo,
                                       disable_local_echo_default);
  bool render_to_associated_sink =
      SelectBool(candidates.render_to_associated_sink_set(),
                 basic_constraint_set.render_to_associated_sink, false);

  AudioProcessingProperties audio_processing_properties =
      SelectAudioProcessingProperties(candidates, basic_constraint_set,
                                      device_info.parameters(),
                                      is_device_capture);

  return AudioCaptureSettings(device_info.device_id(), device_info.parameters(),
                              hotword_enabled, disable_local_echo,
                              render_to_associated_sink,
                              audio_processing_properties);
}

}  // namespace

AudioCaptureSettings SelectSettingsAudioCapture(
    const AudioDeviceCaptureCapabilities& capabilities,
    const blink::WebMediaConstraints& constraints) {
  std::string media_stream_source = GetMediaStreamSource(constraints);
  bool is_device_capture = media_stream_source.empty();
  if (is_device_capture && capabilities.empty())
    return AudioCaptureSettings();

  AudioCaptureCandidates candidates(constraints.Basic(), capabilities,
                                    is_device_capture);
  if (candidates.IsEmpty()) {
    return AudioCaptureSettings(candidates.failed_constraint_name());
  }

  for (const auto& advanced_set : constraints.Advanced()) {
    AudioCaptureCandidates advanced_candidates(advanced_set, capabilities,
                                               is_device_capture);
    AudioCaptureCandidates intersection =
        candidates.Intersection(advanced_candidates);
    if (!intersection.IsEmpty())
      candidates = std::move(intersection);
  }
  DCHECK(!candidates.IsEmpty());

  std::string default_device_id;
  if (!capabilities.empty())
    default_device_id = (*capabilities.begin())->device_id;

  return SelectResult(candidates, constraints.Basic(), default_device_id,
                      media_stream_source);
}

}  // namespace content
