// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/rtc_media_constraints.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Return;

namespace content {

namespace {

#if defined(ANDROID)
const int kAudioProcessingSampleRate = 16000;
#else
const int kAudioProcessingSampleRate = 32000;
#endif
const int kAudioProcessingNumberOfChannel = 1;

// The number of packers used for testing.
const int kNumberOfPacketsForTest = 100;

void ReadDataFromSpeechFile(char* data, int length) {
  base::FilePath file;
  CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &file));
  file = file.Append(FILE_PATH_LITERAL("media"))
             .Append(FILE_PATH_LITERAL("test"))
             .Append(FILE_PATH_LITERAL("data"))
             .Append(FILE_PATH_LITERAL("speech_16b_stereo_48kHz.raw"));
  DCHECK(base::PathExists(file));
  int64 data_file_size64 = 0;
  DCHECK(base::GetFileSize(file, &data_file_size64));
  EXPECT_EQ(length, base::ReadFile(file, data, length));
  DCHECK(data_file_size64 > length);
}

void ApplyFixedAudioConstraints(RTCMediaConstraints* constraints) {
  // Constant constraint keys which enables default audio constraints on
  // mediastreams with audio.
  struct {
    const char* key;
    const char* value;
  } static const kDefaultAudioConstraints[] = {
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
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDefaultAudioConstraints); ++i) {
    constraints->AddMandatory(kDefaultAudioConstraints[i].key,
                              kDefaultAudioConstraints[i].value, false);
  }
}

}  // namespace

class MediaStreamAudioProcessorTest : public ::testing::Test {
 public:
  MediaStreamAudioProcessorTest()
      : params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                media::CHANNEL_LAYOUT_STEREO, 48000, 16, 512) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableAudioTrackProcessing);
  }

 protected:
  // Helper method to save duplicated code.
  void ProcessDataAndVerifyFormat(MediaStreamAudioProcessor* audio_processor,
                                  int expected_output_sample_rate,
                                  int expected_output_channels,
                                  int expected_output_buffer_size) {
    // Read the audio data from a file.
    const int packet_size =
        params_.frames_per_buffer() * 2 * params_.channels();
    const size_t length = packet_size * kNumberOfPacketsForTest;
    scoped_ptr<char[]> capture_data(new char[length]);
    ReadDataFromSpeechFile(capture_data.get(), length);
    const int16* data_ptr = reinterpret_cast<const int16*>(capture_data.get());
    scoped_ptr<media::AudioBus> data_bus = media::AudioBus::Create(
        params_.channels(), params_.frames_per_buffer());
    for (int i = 0; i < kNumberOfPacketsForTest; ++i) {
      data_bus->FromInterleaved(data_ptr, data_bus->frames(), 2);
      audio_processor->PushCaptureData(data_bus.get());

      // |audio_processor| does nothing when the audio processing is off in
      // the processor.
      audio_processor->PushRenderData(
          data_ptr,
          params_.sample_rate(), params_.channels(),
          params_.frames_per_buffer(), base::TimeDelta::FromMilliseconds(10));

      int16* output = NULL;
      while(audio_processor->ProcessAndConsumeData(
          base::TimeDelta::FromMilliseconds(10), 255, false, &output)) {
        EXPECT_TRUE(output != NULL);
        EXPECT_EQ(audio_processor->OutputFormat().sample_rate(),
                  expected_output_sample_rate);
        EXPECT_EQ(audio_processor->OutputFormat().channels(),
                  expected_output_channels);
        EXPECT_EQ(audio_processor->OutputFormat().frames_per_buffer(),
                  expected_output_buffer_size);
      }

      data_ptr += params_.frames_per_buffer() * params_.channels();
    }
  }

  media::AudioParameters params_;
};

TEST_F(MediaStreamAudioProcessorTest, WithoutAudioProcessing) {
  // Setup the audio processor with empty constraint.
  RTCMediaConstraints constraints;
  MediaStreamAudioProcessor audio_processor(&constraints);
  audio_processor.SetCaptureFormat(params_);
  EXPECT_FALSE(audio_processor.has_audio_processing());

  ProcessDataAndVerifyFormat(&audio_processor,
                             params_.sample_rate(),
                             params_.channels(),
                             params_.sample_rate() / 100);
}

TEST_F(MediaStreamAudioProcessorTest, WithAudioProcessing) {
  // Setup the audio processor with default constraint.
  RTCMediaConstraints constraints;
  ApplyFixedAudioConstraints(&constraints);
  MediaStreamAudioProcessor audio_processor(&constraints);
  audio_processor.SetCaptureFormat(params_);
  EXPECT_TRUE(audio_processor.has_audio_processing());

  ProcessDataAndVerifyFormat(&audio_processor,
                             kAudioProcessingSampleRate,
                             kAudioProcessingNumberOfChannel,
                             kAudioProcessingSampleRate / 100);
}

}  // namespace content
