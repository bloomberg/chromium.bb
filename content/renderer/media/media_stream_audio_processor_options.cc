// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor_options.h"

#include <stddef.h>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/common/content_features.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/media_stream_source.h"
#include "media/base/audio_parameters.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace content {

const char MediaAudioConstraints::kEchoCancellation[] = "echoCancellation";
const char MediaAudioConstraints::kGoogEchoCancellation[] =
    "googEchoCancellation";
const char MediaAudioConstraints::kGoogExperimentalEchoCancellation[] =
    "googExperimentalEchoCancellation";
const char MediaAudioConstraints::kGoogAutoGainControl[] =
    "googAutoGainControl";
const char MediaAudioConstraints::kGoogExperimentalAutoGainControl[] =
    "googExperimentalAutoGainControl";
const char MediaAudioConstraints::kGoogNoiseSuppression[] =
    "googNoiseSuppression";
const char MediaAudioConstraints::kGoogExperimentalNoiseSuppression[] =
    "googExperimentalNoiseSuppression";
const char MediaAudioConstraints::kGoogBeamforming[] = "googBeamforming";
const char MediaAudioConstraints::kGoogArrayGeometry[] = "googArrayGeometry";
const char MediaAudioConstraints::kGoogHighpassFilter[] = "googHighpassFilter";
const char MediaAudioConstraints::kGoogTypingNoiseDetection[] =
    "googTypingNoiseDetection";
const char MediaAudioConstraints::kGoogAudioMirroring[] = "googAudioMirroring";

namespace {

// Controls whether the hotword audio stream is used on supported platforms.
const char kMediaStreamAudioHotword[] = "googHotword";

// Constant constraint keys which enables default audio constraints on
// mediastreams with audio.
struct {
  const char* key;
  bool value;
} const kDefaultAudioConstraints[] = {
  { MediaAudioConstraints::kEchoCancellation, true },
  { MediaAudioConstraints::kGoogEchoCancellation, true },
#if defined(OS_ANDROID)
  { MediaAudioConstraints::kGoogExperimentalEchoCancellation, false },
#else
  // Enable the extended filter mode AEC on all non-mobile platforms.
  { MediaAudioConstraints::kGoogExperimentalEchoCancellation, true },
#endif
  { MediaAudioConstraints::kGoogAutoGainControl, true },
  { MediaAudioConstraints::kGoogExperimentalAutoGainControl, true },
  { MediaAudioConstraints::kGoogNoiseSuppression, true },
  { MediaAudioConstraints::kGoogHighpassFilter, true },
  { MediaAudioConstraints::kGoogTypingNoiseDetection, true },
  { MediaAudioConstraints::kGoogExperimentalNoiseSuppression, true },
  // Beamforming will only be enabled if we are also provided with a
  // multi-microphone geometry.
  { MediaAudioConstraints::kGoogBeamforming, true },
  { kMediaStreamAudioHotword, false },
};

// Used to log echo quality based on delay estimates.
enum DelayBasedEchoQuality {
  DELAY_BASED_ECHO_QUALITY_GOOD = 0,
  DELAY_BASED_ECHO_QUALITY_SPURIOUS,
  DELAY_BASED_ECHO_QUALITY_BAD,
  DELAY_BASED_ECHO_QUALITY_INVALID,
  DELAY_BASED_ECHO_QUALITY_MAX
};

DelayBasedEchoQuality EchoDelayFrequencyToQuality(float delay_frequency) {
  const float kEchoDelayFrequencyLowerLimit = 0.1f;
  const float kEchoDelayFrequencyUpperLimit = 0.8f;
  // DELAY_BASED_ECHO_QUALITY_GOOD
  //   delay is out of bounds during at most 10 % of the time.
  // DELAY_BASED_ECHO_QUALITY_SPURIOUS
  //   delay is out of bounds 10-80 % of the time.
  // DELAY_BASED_ECHO_QUALITY_BAD
  //   delay is mostly out of bounds >= 80 % of the time.
  // DELAY_BASED_ECHO_QUALITY_INVALID
  //   delay_frequency is negative which happens if we have insufficient data.
  if (delay_frequency < 0)
    return DELAY_BASED_ECHO_QUALITY_INVALID;
  else if (delay_frequency <= kEchoDelayFrequencyLowerLimit)
    return DELAY_BASED_ECHO_QUALITY_GOOD;
  else if (delay_frequency < kEchoDelayFrequencyUpperLimit)
    return DELAY_BASED_ECHO_QUALITY_SPURIOUS;
  else
    return DELAY_BASED_ECHO_QUALITY_BAD;
}

// Scan the basic and advanced constraints until a value is found.
// If nothing is found, the default is returned.
// Argument 2 is a pointer to class data member.
bool ScanConstraintsForBoolean(
    const blink::WebMediaConstraints& constraints,
    blink::BooleanConstraint blink::WebMediaTrackConstraintSet::*picker,
    bool the_default) {
  bool value;
  if (GetConstraintValueAsBoolean(constraints, picker, &value)) {
    return value;
  }
  return the_default;
}

}  // namespace

MediaAudioConstraints::MediaAudioConstraints(
    const blink::WebMediaConstraints& constraints, int effects)
    : constraints_(constraints),
      effects_(effects),
      default_audio_processing_constraint_value_(true) {
  DCHECK(IsOldAudioConstraints());
  // The default audio processing constraints are turned off when
  // - gUM has a specific kMediaStreamSource, which is used by tab capture
  //   and screen capture.
  // - |kEchoCancellation| is explicitly set to false.
  bool echo_constraint;
  std::string source_string;
  if (GetConstraintValueAsString(
          constraints, &blink::WebMediaTrackConstraintSet::media_stream_source,
          &source_string) ||
      (GetConstraintValueAsBoolean(
           constraints, &blink::WebMediaTrackConstraintSet::echo_cancellation,
           &echo_constraint) &&
       echo_constraint == false)) {
    default_audio_processing_constraint_value_ = false;
  }
}

MediaAudioConstraints::~MediaAudioConstraints() {}

bool MediaAudioConstraints::GetEchoCancellationProperty() const {
  // If platform echo canceller is enabled, disable the software AEC.
  if (effects_ & media::AudioParameters::ECHO_CANCELLER)
    return false;

  // If |kEchoCancellation| is specified in the constraints, it will
  // override the value of |kGoogEchoCancellation|.
  bool echo_value;
  if (GetConstraintValueAsBoolean(
          constraints_, &blink::WebMediaTrackConstraintSet::echo_cancellation,
          &echo_value)) {
    return echo_value;
  }
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_echo_cancellation,
      GetDefaultValueForConstraint(kGoogEchoCancellation));
}

bool MediaAudioConstraints::IsValid() const {
  std::vector<std::string> legal_names(
      {constraints_.Basic().media_stream_source.GetName(),
       constraints_.Basic().device_id.GetName(),
       constraints_.Basic().render_to_associated_sink.GetName()});
  for (size_t j = 0; j < arraysize(kDefaultAudioConstraints); ++j) {
    legal_names.push_back(kDefaultAudioConstraints[j].key);
  }
  std::string failing_name;
  if (constraints_.Basic().HasMandatoryOutsideSet(legal_names, failing_name)) {
    DLOG(ERROR) << "Invalid MediaStream constraint for audio. Name: "
                << failing_name;
    return false;
  }
  return true;
}

bool MediaAudioConstraints::GetDefaultValueForConstraint(
    const std::string& key) const {
  if (!default_audio_processing_constraint_value_)
    return false;

  for (size_t i = 0; i < arraysize(kDefaultAudioConstraints); ++i) {
    if (kDefaultAudioConstraints[i].key == key)
      return kDefaultAudioConstraints[i].value;
  }

  return false;
}

bool MediaAudioConstraints::GetGoogAudioMirroring() const {
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_audio_mirroring,
      GetDefaultValueForConstraint(kGoogAudioMirroring));
}

bool MediaAudioConstraints::GetGoogAutoGainControl() const {
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_auto_gain_control,
      GetDefaultValueForConstraint(kGoogAutoGainControl));
}

bool MediaAudioConstraints::GetGoogExperimentalEchoCancellation() const {
  return ScanConstraintsForBoolean(
      constraints_,
      &blink::WebMediaTrackConstraintSet::goog_experimental_echo_cancellation,
      GetDefaultValueForConstraint(kGoogExperimentalEchoCancellation));
}

bool MediaAudioConstraints::GetGoogTypingNoiseDetection() const {
  return ScanConstraintsForBoolean(
      constraints_,
      &blink::WebMediaTrackConstraintSet::goog_typing_noise_detection,
      GetDefaultValueForConstraint(kGoogTypingNoiseDetection));
}
bool MediaAudioConstraints::GetGoogNoiseSuppression() const {
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_noise_suppression,
      GetDefaultValueForConstraint(kGoogNoiseSuppression));
}

bool MediaAudioConstraints::GetGoogExperimentalNoiseSuppression() const {
  return ScanConstraintsForBoolean(
      constraints_,
      &blink::WebMediaTrackConstraintSet::goog_experimental_noise_suppression,
      GetDefaultValueForConstraint(kGoogExperimentalNoiseSuppression));
}

bool MediaAudioConstraints::GetGoogBeamforming() const {
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_beamforming,
      GetDefaultValueForConstraint(kGoogBeamforming));
}

bool MediaAudioConstraints::GetGoogHighpassFilter() const {
  return ScanConstraintsForBoolean(
      constraints_, &blink::WebMediaTrackConstraintSet::goog_highpass_filter,
      GetDefaultValueForConstraint(kGoogHighpassFilter));
}

bool MediaAudioConstraints::GetGoogExperimentalAutoGainControl() const {
  return ScanConstraintsForBoolean(
      constraints_,
      &blink::WebMediaTrackConstraintSet::goog_experimental_auto_gain_control,
      GetDefaultValueForConstraint(kGoogExperimentalAutoGainControl));
}

std::string MediaAudioConstraints::GetGoogArrayGeometry() const {
  std::string the_value;
  if (GetConstraintValueAsString(
          constraints_, &blink::WebMediaTrackConstraintSet::goog_array_geometry,
          &the_value)) {
    return the_value;
  }
  return "";
}

AudioProcessingProperties::AudioProcessingProperties() = default;
AudioProcessingProperties::AudioProcessingProperties(
    const AudioProcessingProperties& other) = default;
AudioProcessingProperties& AudioProcessingProperties::operator=(
    const AudioProcessingProperties& other) = default;
AudioProcessingProperties::AudioProcessingProperties(
    AudioProcessingProperties&& other) = default;
AudioProcessingProperties& AudioProcessingProperties::operator=(
    AudioProcessingProperties&& other) = default;
AudioProcessingProperties::~AudioProcessingProperties() = default;

void AudioProcessingProperties::DisableDefaultPropertiesForTesting() {
  enable_sw_echo_cancellation = false;
  goog_auto_gain_control = false;
  goog_experimental_echo_cancellation = false;
  goog_typing_noise_detection = false;
  goog_noise_suppression = false;
  goog_experimental_noise_suppression = false;
  goog_beamforming = false;
  goog_highpass_filter = false;
  goog_experimental_auto_gain_control = false;
}

// static
AudioProcessingProperties AudioProcessingProperties::FromConstraints(
    const blink::WebMediaConstraints& constraints,
    const media::AudioParameters& input_params) {
  DCHECK(IsOldAudioConstraints());
  MediaAudioConstraints audio_constraints(constraints, input_params.effects());
  AudioProcessingProperties properties;
  properties.enable_sw_echo_cancellation =
      audio_constraints.GetEchoCancellationProperty();
  // |properties.disable_hw_echo_cancellation| is not used when
  // IsOldAudioConstraints() is true.
  properties.goog_audio_mirroring = audio_constraints.GetGoogAudioMirroring();
  properties.goog_auto_gain_control =
      audio_constraints.GetGoogAutoGainControl();
  properties.goog_experimental_echo_cancellation =
      audio_constraints.GetGoogExperimentalEchoCancellation();
  properties.goog_typing_noise_detection =
      audio_constraints.GetGoogTypingNoiseDetection();
  properties.goog_noise_suppression =
      audio_constraints.GetGoogNoiseSuppression();
  properties.goog_experimental_noise_suppression =
      audio_constraints.GetGoogExperimentalNoiseSuppression();
  properties.goog_beamforming = audio_constraints.GetGoogBeamforming();
  properties.goog_highpass_filter = audio_constraints.GetGoogHighpassFilter();
  properties.goog_experimental_auto_gain_control =
      audio_constraints.GetGoogExperimentalAutoGainControl();
  properties.goog_array_geometry =
      GetArrayGeometryPreferringConstraints(audio_constraints, input_params);

  return properties;
}

EchoInformation::EchoInformation()
    : delay_stats_time_ms_(0),
      echo_frames_received_(false),
      divergent_filter_stats_time_ms_(0),
      num_divergent_filter_fraction_(0),
      num_non_zero_divergent_filter_fraction_(0) {}

EchoInformation::~EchoInformation() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReportAndResetAecDivergentFilterStats();
}

void EchoInformation::UpdateAecStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!echo_cancellation->is_enabled())
    return;

  UpdateAecDelayStats(echo_cancellation);
  UpdateAecDivergentFilterStats(echo_cancellation);
}

void EchoInformation::UpdateAecDelayStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Only start collecting stats if we know echo cancellation has measured an
  // echo. Otherwise we clutter the stats with for example cases where only the
  // microphone is used.
  if (!echo_frames_received_ & !echo_cancellation->stream_has_echo())
    return;

  echo_frames_received_ = true;

  // In WebRTC, three echo delay metrics are calculated and updated every
  // five seconds. We use one of them, |fraction_poor_delays| to log in a UMA
  // histogram an Echo Cancellation quality metric. The stat in WebRTC has a
  // fixed aggregation window of five seconds, so we use the same query
  // frequency to avoid logging old values.
  if (!echo_cancellation->is_delay_logging_enabled())
    return;

  delay_stats_time_ms_ += webrtc::AudioProcessing::kChunkSizeMs;
  if (delay_stats_time_ms_ <
      500 * webrtc::AudioProcessing::kChunkSizeMs) {  // 5 seconds
    return;
  }

  int dummy_median = 0, dummy_std = 0;
  float fraction_poor_delays = 0;
  if (echo_cancellation->GetDelayMetrics(
          &dummy_median, &dummy_std, &fraction_poor_delays) ==
      webrtc::AudioProcessing::kNoError) {
    delay_stats_time_ms_ = 0;
    // Map |fraction_poor_delays| to an Echo Cancellation quality and log in UMA
    // histogram. See DelayBasedEchoQuality for information on histogram
    // buckets.
    UMA_HISTOGRAM_ENUMERATION("WebRTC.AecDelayBasedQuality",
                              EchoDelayFrequencyToQuality(fraction_poor_delays),
                              DELAY_BASED_ECHO_QUALITY_MAX);
  }
}

void EchoInformation::UpdateAecDivergentFilterStats(
    webrtc::EchoCancellation* echo_cancellation) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!echo_cancellation->are_metrics_enabled())
    return;

  divergent_filter_stats_time_ms_ += webrtc::AudioProcessing::kChunkSizeMs;
  if (divergent_filter_stats_time_ms_ <
      100 * webrtc::AudioProcessing::kChunkSizeMs) {  // 1 second
    return;
  }

  webrtc::EchoCancellation::Metrics metrics;
  if (echo_cancellation->GetMetrics(&metrics) ==
      webrtc::AudioProcessing::kNoError) {
    // If not yet calculated, |metrics.divergent_filter_fraction| is -1.0. After
    // being calculated the first time, it is updated periodically.
    if (metrics.divergent_filter_fraction < 0.0f) {
      DCHECK_EQ(num_divergent_filter_fraction_, 0);
      return;
    }
    if (metrics.divergent_filter_fraction > 0.0f) {
      ++num_non_zero_divergent_filter_fraction_;
    }
  } else {
    DLOG(WARNING) << "Get echo cancellation metrics failed.";
  }
  ++num_divergent_filter_fraction_;
  divergent_filter_stats_time_ms_ = 0;
}

void EchoInformation::ReportAndResetAecDivergentFilterStats() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (num_divergent_filter_fraction_ == 0)
    return;

  int non_zero_percent = 100 * num_non_zero_divergent_filter_fraction_ /
                         num_divergent_filter_fraction_;
  UMA_HISTOGRAM_PERCENTAGE("WebRTC.AecFilterHasDivergence", non_zero_percent);

  divergent_filter_stats_time_ms_ = 0;
  num_non_zero_divergent_filter_fraction_ = 0;
  num_divergent_filter_fraction_ = 0;
}

void EnableEchoCancellation(AudioProcessing* audio_processing) {
#if defined(OS_ANDROID)
  // Mobile devices are using AECM.
  CHECK_EQ(0, audio_processing->echo_control_mobile()->set_routing_mode(
                  webrtc::EchoControlMobile::kSpeakerphone));
  CHECK_EQ(0, audio_processing->echo_control_mobile()->Enable(true));
  return;
#endif
  int err = audio_processing->echo_cancellation()->set_suppression_level(
      webrtc::EchoCancellation::kHighSuppression);

  // Enable the metrics for AEC.
  err |= audio_processing->echo_cancellation()->enable_metrics(true);
  err |= audio_processing->echo_cancellation()->enable_delay_logging(true);
  err |= audio_processing->echo_cancellation()->Enable(true);
  CHECK_EQ(err, 0);
}

void EnableNoiseSuppression(AudioProcessing* audio_processing,
                            webrtc::NoiseSuppression::Level ns_level) {
  int err = audio_processing->noise_suppression()->set_level(ns_level);
  err |= audio_processing->noise_suppression()->Enable(true);
  CHECK_EQ(err, 0);
}

void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector) {
  int err = audio_processing->voice_detection()->Enable(true);
  err |= audio_processing->voice_detection()->set_likelihood(
      webrtc::VoiceDetection::kVeryLowLikelihood);
  CHECK_EQ(err, 0);

  // Configure the update period to 1s (100 * 10ms) in the typing detector.
  typing_detector->SetParameters(0, 0, 0, 0, 0, 100);
}

void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               rtc::TaskQueue* worker_queue) {
  DCHECK(aec_dump_file.IsValid());

  FILE* stream = base::FileToFILE(std::move(aec_dump_file), "w");
  if (!stream) {
    LOG(DFATAL) << "Failed to open AEC dump file";
    return;
  }

  auto aec_dump = webrtc::AecDumpFactory::Create(
      stream, -1 /* max_log_size_bytes */, worker_queue);
  if (!aec_dump) {
    LOG(ERROR) << "Failed to start AEC debug recording";
    return;
  }
  audio_processing->AttachAecDump(std::move(aec_dump));
}

void StopEchoCancellationDump(AudioProcessing* audio_processing) {
  audio_processing->DetachAecDump();
}

void EnableAutomaticGainControl(AudioProcessing* audio_processing) {
#if defined(OS_ANDROID)
  const webrtc::GainControl::Mode mode = webrtc::GainControl::kFixedDigital;
#else
  const webrtc::GainControl::Mode mode = webrtc::GainControl::kAdaptiveAnalog;
#endif
  int err = audio_processing->gain_control()->set_mode(mode);
  err |= audio_processing->gain_control()->Enable(true);
  CHECK_EQ(err, 0);
}

void GetAudioProcessingStats(
    AudioProcessing* audio_processing,
    webrtc::AudioProcessorInterface::AudioProcessorStats* stats) {
  // TODO(ivoc): Change the APM stats to use rtc::Optional instead of default
  //             values.
  auto apm_stats = audio_processing->GetStatistics();
  stats->echo_return_loss = apm_stats.echo_return_loss.instant();
  stats->echo_return_loss_enhancement =
      apm_stats.echo_return_loss_enhancement.instant();
  stats->aec_divergent_filter_fraction = apm_stats.divergent_filter_fraction;

  stats->echo_delay_median_ms = apm_stats.delay_median;
  stats->echo_delay_std_ms = apm_stats.delay_standard_deviation;

  stats->residual_echo_likelihood = apm_stats.residual_echo_likelihood;
  stats->residual_echo_likelihood_recent_max =
      apm_stats.residual_echo_likelihood_recent_max;
}

std::vector<media::Point> GetArrayGeometryPreferringConstraints(
    const MediaAudioConstraints& audio_constraints,
    const media::AudioParameters& input_params) {
  const std::string constraints_geometry =
      audio_constraints.GetGoogArrayGeometry();

  // Give preference to the audio constraint over the device-supplied mic
  // positions. This is mainly for testing purposes.
  return constraints_geometry.empty()
             ? input_params.mic_positions()
             : media::ParsePointsFromString(constraints_geometry);
}

bool IsOldAudioConstraints() {
  return base::FeatureList::IsEnabled(
      features::kMediaStreamOldAudioConstraints);
}

}  // namespace content
