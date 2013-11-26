// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_

#include <string>

namespace webrtc {

class AudioFrame;
class AudioProcessing;
class MediaConstraintsInterface;

}

namespace content {

using webrtc::AudioProcessing;
using webrtc::MediaConstraintsInterface;

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

// Enables the high pass filter in |audio_processing|.
void EnableHighPassFilter(AudioProcessing* audio_processing);

// Enables the typing detection in |audio_processing|.
void EnableTypingDetection(AudioProcessing* audio_processing);

// Enables the experimental echo cancellation in |audio_processing|.
void EnableExperimentalEchoCancellation(AudioProcessing* audio_processing);

// Starts the echo cancellation dump in |audio_processing|.
void StartAecDump(AudioProcessing* audio_processing);

// Stops the echo cancellation dump in |audio_processing|.
void StopAecDump(AudioProcessing* audio_processing);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_AUDIO_PROCESSOR_OPTIONS_H_
