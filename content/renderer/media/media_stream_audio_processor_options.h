// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

namespace blink {
class WebMediaConstraints;
}

namespace webrtc {

class AudioFrame;
class AudioProcessing;
class MediaConstraintsInterface;
class TypingDetection;

}

namespace content {

class RTCMediaConstraints;

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

// Merge |constraints| with |kDefaultAudioConstraints|. For any key which exists
// in both, the value from |constraints| is maintained, including its
// mandatory/optional status. New values from |kDefaultAudioConstraints| will
// be added with mandatory status.
void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints);

// Checks if any audio constraints are set that requires audio processing to
// be applied. |effects| is the bitmasks telling whether certain platform
// hardware audio effects are enabled, like hardware echo cancellation. If some
// hardware effect is enabled, the corresponding software audio processing will
// be disabled.
bool NeedsAudioProcessing(const blink::WebMediaConstraints& constraints,
                          int effects);

// Gets the property named by |key| from the |constraints|.
// Returns true if the key is found and has a valid boolean value; Otherwise
// false.
bool GetPropertyFromConstraints(
    const MediaConstraintsInterface* constraints,
    const std::string& key);

// Enables the echo cancellation in |audio_processing|.
void EnableEchoCancellation(AudioProcessing* audio_processing);

// Enables the noise suppression in |audio_processing|.
void EnableNoiseSuppression(AudioProcessing* audio_processing);

// Enables the experimental noise suppression in |audio_processing|.
void EnableExperimentalNoiseSuppression(AudioProcessing* audio_processing);

// Enables the high pass filter in |audio_processing|.
void EnableHighPassFilter(AudioProcessing* audio_processing);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector);

// Enables the experimental echo cancellation in |audio_processing|.
void EnableExperimentalEchoCancellation(AudioProcessing* audio_processing);

// Starts the echo cancellation dump in |audio_processing|.
void StartAecDump(AudioProcessing* audio_processing);

// Stops the echo cancellation dump in |audio_processing|.
void StopAecDump(AudioProcessing* audio_processing);

void EnableAutomaticGainControl(AudioProcessing* audio_processing);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
