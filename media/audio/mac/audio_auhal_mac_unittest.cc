// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/simple_sources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;

static const int kBitsPerSample = 16;

// TODO(crogers): Most of these tests can be made platform agnostic.
// http://crbug.com/223242

namespace media {

class MockAudioSourceCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD2(OnMoreData, int(AudioBus* audio_bus,
                               AudioBuffersState buffers_state));
  MOCK_METHOD3(OnMoreIOData, int(AudioBus* source,
                                 AudioBus* dest,
                                 AudioBuffersState buffers_state));
  MOCK_METHOD1(OnError, void(AudioOutputStream* stream));
};

// Convenience method which creates a default AudioOutputStream object but
// also allows the user to modify the default settings.
class AudioOutputStreamWrapper {
 public:
  explicit AudioOutputStreamWrapper()
      : audio_man_(AudioManager::Create()),
        format_(AudioParameters::AUDIO_PCM_LOW_LATENCY),
        bits_per_sample_(kBitsPerSample) {
    AudioParameters preferred_params =
        audio_man_->GetDefaultOutputStreamParameters();
    channel_layout_ = preferred_params.channel_layout();
    channels_ = preferred_params.channels();
    sample_rate_ = preferred_params.sample_rate();
    samples_per_packet_ = preferred_params.frames_per_buffer();
  }

  ~AudioOutputStreamWrapper() {}

  // Creates AudioOutputStream object using default parameters.
  AudioOutputStream* Create() {
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // frame size is modified.
  AudioOutputStream* Create(int samples_per_packet) {
    samples_per_packet_ = samples_per_packet;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // sample rate is modified.
  AudioOutputStream* CreateWithSampleRate(int sample_rate) {
    sample_rate_ = sample_rate;
    return CreateOutputStream();
  }

  // Creates AudioOutputStream object using non-default parameters where the
  // channel layout is modified.
  AudioOutputStream* CreateWithLayout(ChannelLayout layout) {
    channel_layout_ = layout;
    channels_ = ChannelLayoutToChannelCount(layout);
    return CreateOutputStream();
  }

  AudioParameters::Format format() const { return format_; }
  int channels() const { return ChannelLayoutToChannelCount(channel_layout_); }
  int bits_per_sample() const { return bits_per_sample_; }
  int sample_rate() const { return sample_rate_; }
  int samples_per_packet() const { return samples_per_packet_; }

  bool CanRunAudioTests() {
    return audio_man_->HasAudioOutputDevices();
  }

 private:
  AudioOutputStream* CreateOutputStream() {
    AudioParameters params;
    params.Reset(format_, channel_layout_,
                 channels_, 0,
                 sample_rate_, bits_per_sample_,
                 samples_per_packet_);

    AudioOutputStream* aos = audio_man_->MakeAudioOutputStream(params);
    EXPECT_TRUE(aos);
    return aos;
  }

  scoped_ptr<AudioManager> audio_man_;

  AudioParameters::Format format_;
  ChannelLayout channel_layout_;
  int channels_;
  int bits_per_sample_;
  int sample_rate_;
  int samples_per_packet_;
};

// Test that we can get the hardware sample-rate.
TEST(AUHALStreamTest, HardwareSampleRate) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  int sample_rate = aosw.sample_rate();
  EXPECT_GE(sample_rate, 16000);
  EXPECT_LE(sample_rate, 192000);
}

// Test Create(), Close() calling sequence.
TEST(AUHALStreamTest, CreateAndClose) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  AudioOutputStream* aos = aosw.Create();
  aos->Close();
}

// Test Open(), Close() calling sequence.
TEST(AUHALStreamTest, OpenAndClose) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Test Open(), Start(), Close() calling sequence.
TEST(AUHALStreamTest, OpenStartAndClose) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos))
      .Times(0);
  aos->Start(&source);
  aos->Close();
}

// Test Open(), Start(), Stop(), Close() calling sequence.
TEST(AUHALStreamTest, OpenStartStopAndClose) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  AudioOutputStream* aos = aosw.Create();
  EXPECT_TRUE(aos->Open());
  MockAudioSourceCallback source;
  EXPECT_CALL(source, OnError(aos))
      .Times(0);
  aos->Start(&source);
  aos->Stop();
  aos->Close();
}

// This test produces actual audio for 0.5 seconds on the default audio device
// at the hardware sample-rate (usually 44.1KHz).
// Parameters have been chosen carefully so you should not hear
// pops or noises while the sound is playing.
TEST(AUHALStreamTest, AUHALStreamPlay200HzTone) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  AudioOutputStream* aos = aosw.CreateWithLayout(CHANNEL_LAYOUT_MONO);

  EXPECT_TRUE(aos->Open());

  SineWaveAudioSource source(1, 200.0, aosw.sample_rate());
  aos->Start(&source);
  usleep(500000);

  aos->Stop();
  aos->Close();
}

// Test that Open() will fail with a sample-rate which isn't the hardware
// sample-rate.
TEST(AUHALStreamTest, AUHALStreamInvalidSampleRate) {
  AudioOutputStreamWrapper aosw;
  if (!aosw.CanRunAudioTests())
    return;

  int non_default_sample_rate = aosw.sample_rate() == 44100 ?
      48000 : 44100;
  AudioOutputStream* aos = aosw.CreateWithSampleRate(non_default_sample_rate);

  EXPECT_FALSE(aos->Open());

  aos->Close();
}

}  // namespace media
