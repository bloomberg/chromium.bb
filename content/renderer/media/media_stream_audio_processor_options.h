// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "media/base/audio_point.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/media/base/mediachannel.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc_overrides/webrtc/rtc_base/task_queue.h"

namespace webrtc {

class EchoCancellation;
class TypingDetection;

}

namespace content {

using webrtc::AudioProcessing;

// A helper class to parse audio constraints from a blink::WebMediaConstraints
// object.
// TODO(guidou): Remove this class. http://crbug.com/706408
class CONTENT_EXPORT MediaAudioConstraints {
 public:
  // Constraint keys used by audio processing.
  static const char kEchoCancellation[];
  static const char kGoogEchoCancellation[];
  static const char kGoogExperimentalEchoCancellation[];
  static const char kGoogAutoGainControl[];
  static const char kGoogExperimentalAutoGainControl[];
  static const char kGoogNoiseSuppression[];
  static const char kGoogExperimentalNoiseSuppression[];
  static const char kGoogBeamforming[];
  static const char kGoogArrayGeometry[];
  static const char kGoogHighpassFilter[];
  static const char kGoogTypingNoiseDetection[];
  static const char kGoogAudioMirroring[];

  // Merge |options| with |kDefaultAudioConstraints|. For any key which
  // exists in both, the value from |options| is maintained.
  // New values from |kDefaultAudioConstraints| will
  // be added.
  // TODO(hta): Switch to an interface without "cricket::" when webrtc has one.


  // |effects| is the bitmasks telling whether certain platform
  // hardware audio effects are enabled, like hardware echo cancellation. If
  // some hardware effect is enabled, the corresponding software audio
  // processing will be disabled.
  MediaAudioConstraints(const blink::WebMediaConstraints& constraints,
                        int effects);
  virtual ~MediaAudioConstraints();

  bool GetGoogAudioMirroring() const;
  bool GetGoogAutoGainControl() const;
  bool GetGoogExperimentalEchoCancellation() const;
  bool GetGoogTypingNoiseDetection() const;
  bool GetGoogNoiseSuppression() const;
  bool GetGoogExperimentalNoiseSuppression() const;
  bool GetGoogBeamforming() const;
  bool GetGoogHighpassFilter() const;
  bool GetGoogExperimentalAutoGainControl() const;
  std::string GetGoogArrayGeometry() const;

  // Gets the property of echo cancellation defined in |constraints_|. The
  // returned value depends on a combination of |effects_|, |kEchoCancellation|
  // and |kGoogEchoCancellation| in |constraints_|.
  bool GetEchoCancellationProperty() const;

  // Returns true if all the mandatory constraints in |constraints_| are valid;
  // Otherwise return false.
  bool IsValid() const;

  // Exposed for testing.
  bool default_audio_processing_constraint_value() const {
    return default_audio_processing_constraint_value_;
  }

 private:
  // Gets the default value of constraint named by |key| in |constraints|.
  bool GetDefaultValueForConstraint(const std::string& key) const;
  const blink::WebMediaConstraints constraints_;
  const int effects_;
  bool default_audio_processing_constraint_value_;
};

// Simple struct with audio-processing properties. Will substitute
// MediaAudioConstraints once the old constraints-processing algorithm is
// removed. http://crbug.com/706408
struct CONTENT_EXPORT AudioProcessingProperties {
  // Creates an AudioProcessingProperties object with fields initialized to
  // their default values.
  AudioProcessingProperties();
  AudioProcessingProperties(const AudioProcessingProperties& other);
  AudioProcessingProperties& operator=(const AudioProcessingProperties& other);
  AudioProcessingProperties(AudioProcessingProperties&& other);
  AudioProcessingProperties& operator=(AudioProcessingProperties&& other);
  ~AudioProcessingProperties();

  // Disables properties that are enabled by default.
  void DisableDefaultPropertiesForTesting();

  // TODO(guidou): Remove this function. http://crbug.com/706408
  static AudioProcessingProperties FromConstraints(
      const blink::WebMediaConstraints& constraints,
      const media::AudioParameters& input_params);

  bool enable_sw_echo_cancellation = true;
  bool disable_hw_echo_cancellation = false;
  bool goog_audio_mirroring = false;
  bool goog_auto_gain_control = true;
  bool goog_experimental_echo_cancellation =
#if defined(OS_ANDROID)
      false;
#else
      true;
#endif
  bool goog_typing_noise_detection = true;
  bool goog_noise_suppression = true;
  bool goog_experimental_noise_suppression = true;
  bool goog_beamforming = true;
  bool goog_highpass_filter = true;
  bool goog_experimental_auto_gain_control = true;
  std::vector<media::Point> goog_array_geometry;
};

// A helper class to log echo information in general and Echo Cancellation
// quality in particular.
class CONTENT_EXPORT EchoInformation {
 public:
  EchoInformation();
  virtual ~EchoInformation();

  // Updates stats, and reports delay metrics as UMA stats every 5 seconds.
  // Must be called every time AudioProcessing::ProcessStream() is called.
  void UpdateAecStats(webrtc::EchoCancellation* echo_cancellation);

  // Reports AEC divergent filter metrics as UMA and resets the associated data.
  void ReportAndResetAecDivergentFilterStats();

 private:
  void UpdateAecDelayStats(webrtc::EchoCancellation* echo_cancellation);
  void UpdateAecDivergentFilterStats(
      webrtc::EchoCancellation* echo_cancellation);

  // Counter to track 5 seconds of data in order to query a new metric from
  // webrtc::EchoCancellation::GetEchoDelayMetrics().
  int delay_stats_time_ms_;
  bool echo_frames_received_;

  // Counter to track 1 second of data in order to query a new divergent filter
  // fraction metric from webrtc::EchoCancellation::GetMetrics().
  int divergent_filter_stats_time_ms_;

  // Total number of times we queried for the divergent filter fraction metric.
  int num_divergent_filter_fraction_;

  // Number of non-zero divergent filter fraction metrics.
  int num_non_zero_divergent_filter_fraction_;

  // Ensures that this class is accessed on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(EchoInformation);
};

// Enables the echo cancellation in |audio_processing|.
void EnableEchoCancellation(AudioProcessing* audio_processing);

// Enables the noise suppression in |audio_processing|.
void EnableNoiseSuppression(AudioProcessing* audio_processing,
                            webrtc::NoiseSuppression::Level ns_level);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector);

// Starts the echo cancellation dump in
// |audio_processing|. |worker_queue| must be kept alive until either
// |audio_processing| is destroyed, or
// StopEchoCancellationDump(audio_processing) is called.
void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file,
                               rtc::TaskQueue* worker_queue);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
void StopEchoCancellationDump(AudioProcessing* audio_processing);

void EnableAutomaticGainControl(AudioProcessing* audio_processing);

void GetAudioProcessingStats(
    AudioProcessing* audio_processing,
    webrtc::AudioProcessorInterface::AudioProcessorStats* stats);

// Returns the array geometry from the media constraints if existing and
// otherwise that provided by the input device.
// TODO(guidou): Remove this function. http://crbug.com/706408
CONTENT_EXPORT std::vector<media::Point> GetArrayGeometryPreferringConstraints(
    const MediaAudioConstraints& audio_constraints,
    const media::AudioParameters& input_params);

// TODO(guidou): Remove this function. http://crbug.com/706408
CONTENT_EXPORT bool IsOldAudioConstraints();

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
