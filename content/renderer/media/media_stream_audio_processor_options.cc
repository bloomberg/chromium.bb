// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_audio_processor_options.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/media/media_stream_options.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "media/audio/audio_parameters.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediaconstraintsinterface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/modules/audio_processing/typing_detection.h"

namespace content {

namespace {

// Constant constraint keys which enables default audio constraints on
// mediastreams with audio.
struct {
  const char* key;
  const char* value;
} const kDefaultAudioConstraints[] = {
  { webrtc::MediaConstraintsInterface::kEchoCancellation,
    webrtc::MediaConstraintsInterface::kValueTrue },
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  // Enable the extended filter mode AEC on platforms with known echo issues.
  { webrtc::MediaConstraintsInterface::kExperimentalEchoCancellation,
    webrtc::MediaConstraintsInterface::kValueTrue },
#endif
  { webrtc::MediaConstraintsInterface::kAutoGainControl,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kExperimentalAutoGainControl,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kNoiseSuppression,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kHighpassFilter,
    webrtc::MediaConstraintsInterface::kValueTrue },
  { webrtc::MediaConstraintsInterface::kTypingNoiseDetection,
    webrtc::MediaConstraintsInterface::kValueTrue },
#if defined(OS_WIN)
  { content::kMediaStreamAudioDucking,
    webrtc::MediaConstraintsInterface::kValueTrue },
#endif
};

} // namespace

void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDefaultAudioConstraints); ++i) {
    bool already_set_value;
    if (!webrtc::FindConstraint(constraints, kDefaultAudioConstraints[i].key,
                                &already_set_value, NULL)) {
      constraints->AddOptional(kDefaultAudioConstraints[i].key,
          kDefaultAudioConstraints[i].value, false);
    } else {
      DVLOG(1) << "Constraint " << kDefaultAudioConstraints[i].key
               << " already set to " << already_set_value;
    }
  }
}

bool NeedsAudioProcessing(const blink::WebMediaConstraints& constraints,
                          int effects) {
  RTCMediaConstraints native_constraints(constraints);
  ApplyFixedAudioConstraints(&native_constraints);
  if (effects & media::AudioParameters::ECHO_CANCELLER) {
    // If platform echo canceller is enabled, disable the software AEC.
    native_constraints.AddOptional(
        MediaConstraintsInterface::kEchoCancellation,
        MediaConstraintsInterface::kValueFalse, true);
  }
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDefaultAudioConstraints); ++i) {
    bool value = false;
    if (webrtc::FindConstraint(&native_constraints,
                               kDefaultAudioConstraints[i].key, &value, NULL) &&
        value) {
      return true;
    }
  }

  return false;
}

bool GetPropertyFromConstraints(const MediaConstraintsInterface* constraints,
                                const std::string& key) {
  bool value = false;
  return webrtc::FindConstraint(constraints, key, &value, NULL) && value;
}

void EnableEchoCancellation(AudioProcessing* audio_processing) {
#if defined(OS_ANDROID)
  // Mobile devices are using AECM.
  int err = audio_processing->echo_control_mobile()->set_routing_mode(
      webrtc::EchoControlMobile::kSpeakerphone);
  err |= audio_processing->echo_control_mobile()->Enable(true);
  CHECK_EQ(err, 0);
#else
  int err = audio_processing->echo_cancellation()->set_suppression_level(
      webrtc::EchoCancellation::kHighSuppression);

  // Enable the metrics for AEC.
  err |= audio_processing->echo_cancellation()->enable_metrics(true);
  err |= audio_processing->echo_cancellation()->enable_delay_logging(true);
  err |= audio_processing->echo_cancellation()->Enable(true);
  CHECK_EQ(err, 0);
#endif
}

void EnableNoiseSuppression(AudioProcessing* audio_processing) {
  int err = audio_processing->noise_suppression()->set_level(
      webrtc::NoiseSuppression::kHigh);
  err |= audio_processing->noise_suppression()->Enable(true);
  CHECK_EQ(err, 0);
}

void EnableHighPassFilter(AudioProcessing* audio_processing) {
  CHECK_EQ(audio_processing->high_pass_filter()->Enable(true), 0);
}

void EnableTypingDetection(AudioProcessing* audio_processing,
                           webrtc::TypingDetection* typing_detector) {
  int err = audio_processing->voice_detection()->Enable(true);
  err |= audio_processing->voice_detection()->set_likelihood(
      webrtc::VoiceDetection::kVeryLowLikelihood);
  CHECK_EQ(err, 0);

  // Configure the update period to 100ms (10 * 10ms) in the typing detector.
  typing_detector->SetParameters(0, 0, 0, 0, 0, 10);
}

void EnableExperimentalEchoCancellation(AudioProcessing* audio_processing) {
  webrtc::Config config;
  config.Set<webrtc::DelayCorrection>(new webrtc::DelayCorrection(true));
  audio_processing->SetExtraOptions(config);
}

void StartAecDump(AudioProcessing* audio_processing) {
  // TODO(grunell): Figure out a more suitable directory for the audio dump
  // data.
  base::FilePath path;
#if defined(CHROMEOS)
  PathService::Get(base::DIR_TEMP, &path);
#elif defined(ANDROID)
  path = base::FilePath(FILE_PATH_LITERAL("sdcard"));
#else
  PathService::Get(base::DIR_EXE, &path);
#endif
  base::FilePath file = path.Append(FILE_PATH_LITERAL("audio.aecdump"));

#if defined(OS_WIN)
  const std::string file_name = base::WideToUTF8(file.value());
#else
  const std::string file_name = file.value();
#endif
  if (audio_processing->StartDebugRecording(file_name.c_str()))
    DLOG(ERROR) << "Fail to start AEC debug recording";
}

void StopAecDump(AudioProcessing* audio_processing) {
  if (audio_processing->StopDebugRecording())
    DLOG(ERROR) << "Fail to stop AEC debug recording";
}

void EnableAutomaticGainControl(AudioProcessing* audio_processing) {
#if defined(OS_ANDROID) || defined(OS_IOS)
  const webrtc::GainControl::Mode mode = webrtc::GainControl::kFixedDigital;
#else
  const webrtc::GainControl::Mode mode = webrtc::GainControl::kAdaptiveAnalog;
#endif
  int err = audio_processing->gain_control()->set_mode(mode);
  err |= audio_processing->gain_control()->Enable(true);
  CHECK_EQ(err, 0);
}

}  // namespace content
