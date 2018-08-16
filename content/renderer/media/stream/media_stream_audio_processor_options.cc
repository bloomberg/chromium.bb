// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/media_stream_audio_processor_options.h"

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
#include "content/public/common/content_features.h"
#include "content/renderer/media/stream/media_stream_constraints_util.h"
#include "content/renderer/media/stream/media_stream_source.h"
#include "media/base/audio_parameters.h"
#include "third_party/webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace content {

AudioProcessingProperties::AudioProcessingProperties() = default;
AudioProcessingProperties::AudioProcessingProperties(
    const AudioProcessingProperties& other) = default;
AudioProcessingProperties& AudioProcessingProperties::operator=(
    const AudioProcessingProperties& other) = default;

void AudioProcessingProperties::DisableDefaultProperties() {
  echo_cancellation_type = EchoCancellationType::kEchoCancellationDisabled;
  goog_auto_gain_control = false;
  goog_experimental_echo_cancellation = false;
  goog_typing_noise_detection = false;
  goog_noise_suppression = false;
  goog_experimental_noise_suppression = false;
  goog_highpass_filter = false;
  goog_experimental_auto_gain_control = false;
}

bool AudioProcessingProperties::EchoCancellationEnabled() const {
  return echo_cancellation_type !=
         EchoCancellationType::kEchoCancellationDisabled;
}

bool AudioProcessingProperties::EchoCancellationIsWebRtcProvided() const {
  return echo_cancellation_type ==
             EchoCancellationType::kEchoCancellationAec2 ||
         echo_cancellation_type == EchoCancellationType::kEchoCancellationAec3;
}

void EnableEchoCancellation(AudioProcessing* audio_processing) {
  // TODO(bugs.webrtc.org/9535): Remove double-booking AEC toggle when the
  // config applies (from 2018-08-16).
  webrtc::AudioProcessing::Config apm_config = audio_processing->GetConfig();
  apm_config.echo_canceller.enabled = true;
#if defined(OS_ANDROID)
  // Mobile devices are using AECM.
  CHECK_EQ(0, audio_processing->echo_control_mobile()->set_routing_mode(
                  webrtc::EchoControlMobile::kSpeakerphone));
  CHECK_EQ(0, audio_processing->echo_control_mobile()->Enable(true));
  apm_config.echo_canceller.mobile_mode = true;
#else
  int err = audio_processing->echo_cancellation()->set_suppression_level(
      webrtc::EchoCancellation::kHighSuppression);

  // Enable the metrics for AEC.
  err |= audio_processing->echo_cancellation()->enable_metrics(true);
  err |= audio_processing->echo_cancellation()->enable_delay_logging(true);
  err |= audio_processing->echo_cancellation()->Enable(true);
  CHECK_EQ(err, 0);
  apm_config.echo_canceller.mobile_mode = false;
#endif
  audio_processing->ApplyConfig(apm_config);
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
  // TODO(ivoc): Change the APM stats to use optional instead of default values.
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

}  // namespace content
