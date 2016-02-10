// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/media_stream_request.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/webrtc/api/mediastreaminterface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace webrtc {

class EchoCancellation;
class MediaConstraintsInterface;
class TypingDetection;

}

namespace content {

class RTCMediaConstraints;

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

// A helper class to parse audio constraints from a blink::WebMediaConstraints
// object.
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

  // Merge |constraints| with |kDefaultAudioConstraints|. For any key which
  // exists in both, the value from |constraints| is maintained, including its
  // mandatory/optional status. New values from |kDefaultAudioConstraints| will
  // be added with optional status.
  static void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints);

  // |effects| is the bitmasks telling whether certain platform
  // hardware audio effects are enabled, like hardware echo cancellation. If
  // some hardware effect is enabled, the corresponding software audio
  // processing will be disabled.
  MediaAudioConstraints(const blink::WebMediaConstraints& constraints,
                        int effects);
  virtual ~MediaAudioConstraints();

  // Gets the property of the constraint named by |key| in |constraints_|.
  // Returns the constraint's value if the key is found; otherwise returns the
  // default value of the constraint.
  // Note, for constraint of |kEchoCancellation| or |kGoogEchoCancellation|,
  // clients should use GetEchoCancellationProperty().
  bool GetProperty(const std::string& key) const;

  // Gets the property of the constraint named by |key| in |constraints_| as a
  // string. Returns the constraint's string value if the key is found;
  // otherwise returns an empty string.
  std::string GetPropertyAsString(const std::string& key) const;

  // Gets the property of echo cancellation defined in |constraints_|. The
  // returned value depends on a combination of |effects_|, |kEchoCancellation|
  // and |kGoogEchoCancellation| in |constraints_|.
  bool GetEchoCancellationProperty() const;

  // Returns true if all the mandatory constraints in |constraints_| are valid;
  // Otherwise return false.
  bool IsValid() const;

 private:
  // Gets the default value of constraint named by |key| in |constraints|.
  bool GetDefaultValueForConstraint(
      const blink::WebMediaConstraints& constraints,
      const std::string& key) const;

  const blink::WebMediaConstraints constraints_;
  const int effects_;
  bool default_audio_processing_constraint_value_;
};

// A helper class to log echo information in general and Echo Cancellation
// quality in particular.
class CONTENT_EXPORT EchoInformation {
 public:
  EchoInformation();
  virtual ~EchoInformation();

  void UpdateAecDelayStats(webrtc::EchoCancellation* echo_cancellation);

 private:
  // Counter to track 5 seconds of processed 10 ms chunks in order to query a
  // new metric from webrtc::EchoCancellation::GetEchoDelayMetrics().
  int num_chunks_;
  bool echo_frames_received_;

  DISALLOW_COPY_AND_ASSIGN(EchoInformation);
};

// Enables the echo cancellation in |audio_processing|.
void EnableEchoCancellation(AudioProcessing* audio_processing);

// Enables the noise suppression in |audio_processing|.
void EnableNoiseSuppression(AudioProcessing* audio_processing,
                            webrtc::NoiseSuppression::Level ns_level);

// Enables the high pass filter in |audio_processing|.
void EnableHighPassFilter(AudioProcessing* audio_processing);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector);

// Starts the echo cancellation dump in |audio_processing|.
void StartEchoCancellationDump(AudioProcessing* audio_processing,
                               base::File aec_dump_file);

// Stops the echo cancellation dump in |audio_processing|.
// This method has no impact if echo cancellation dump has not been started on
// |audio_processing|.
void StopEchoCancellationDump(AudioProcessing* audio_processing);

void EnableAutomaticGainControl(AudioProcessing* audio_processing);

void GetAecStats(webrtc::EchoCancellation* echo_cancellation,
                 webrtc::AudioProcessorInterface::AudioProcessorStats* stats);

// Returns the array geometry from the media constraints if existing and
// otherwise that provided by the input device.
CONTENT_EXPORT std::vector<webrtc::Point> GetArrayGeometryPreferringConstraints(
    const MediaAudioConstraints& audio_constraints,
    const MediaStreamDevice::AudioDeviceParameters& input_params);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
