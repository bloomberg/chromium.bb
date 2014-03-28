// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_buffer_converter.h"
#include "media/base/sinc_resampler.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

const int kOutSampleRate = 44100;
const ChannelLayout kOutChannelLayout = CHANNEL_LAYOUT_STEREO;

static scoped_refptr<AudioBuffer> MakeTestBuffer(int sample_rate,
                                                 ChannelLayout channel_layout,
                                                 int frames) {
  return MakeAudioBuffer<uint8>(kSampleFormatU8,
                                channel_layout,
                                sample_rate,
                                0,
                                1,
                                frames,
                                base::TimeDelta::FromSeconds(0),
                                base::TimeDelta::FromSeconds(0));
}

class AudioBufferConverterTest : public ::testing::Test {
 public:
  AudioBufferConverterTest()
      : input_frames_(0), expected_output_frames_(0.0), output_frames_(0) {
    AudioParameters output_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  kOutChannelLayout,
                                  kOutSampleRate,
                                  16,
                                  512);
    audio_buffer_converter_.reset(new AudioBufferConverter(output_params));
  }

  void Reset() {
    audio_buffer_converter_->Reset();
    output_frames_ = expected_output_frames_ = input_frames_ = 0;
  }

  void AddInput(const scoped_refptr<AudioBuffer>& in) {
    if (!in->end_of_stream()) {
      input_frames_ += in->frame_count();
      expected_output_frames_ +=
          in->frame_count() *
          (static_cast<double>(kOutSampleRate) / in->sample_rate());
    }
    audio_buffer_converter_->AddInput(in);
  }

  void ConsumeAllOutput() {
    AddInput(AudioBuffer::CreateEOSBuffer());
    while (audio_buffer_converter_->HasNextBuffer()) {
      scoped_refptr<AudioBuffer> out = audio_buffer_converter_->GetNextBuffer();
      if (!out->end_of_stream()) {
        output_frames_ += out->frame_count();
        EXPECT_EQ(out->sample_rate(), kOutSampleRate);
        EXPECT_EQ(out->channel_layout(), kOutChannelLayout);
      } else {
        EXPECT_FALSE(audio_buffer_converter_->HasNextBuffer());
      }
    }
    EXPECT_EQ(output_frames_, ceil(expected_output_frames_));
  }

 private:
  scoped_ptr<AudioBufferConverter> audio_buffer_converter_;

  int input_frames_;
  double expected_output_frames_;
  int output_frames_;
};

TEST_F(AudioBufferConverterTest, PassThrough) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, kOutChannelLayout, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Downsample) {
  scoped_refptr<AudioBuffer> in = MakeTestBuffer(48000, kOutChannelLayout, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Upsample) {
  scoped_refptr<AudioBuffer> in = MakeTestBuffer(8000, kOutChannelLayout, 512);
  AddInput(in);
  ConsumeAllOutput();
}

// Test resampling a buffer smaller than the SincResampler's kernel size.
TEST_F(AudioBufferConverterTest, Resample_TinyBuffer) {
  AddInput(MakeTestBuffer(
      48000, CHANNEL_LAYOUT_STEREO, SincResampler::kKernelSize - 1));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Resample_DifferingBufferSizes) {
  const int input_sample_rate = 48000;
  AddInput(MakeTestBuffer(input_sample_rate, kOutChannelLayout, 100));
  AddInput(MakeTestBuffer(input_sample_rate, kOutChannelLayout, 200));
  AddInput(MakeTestBuffer(input_sample_rate, kOutChannelLayout, 300));
  AddInput(MakeTestBuffer(input_sample_rate, kOutChannelLayout, 400));
  AddInput(MakeTestBuffer(input_sample_rate, kOutChannelLayout, 500));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ChannelDownmix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_MONO, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ChannelUpmix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_5_1, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResampleAndRemix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(48000, CHANNEL_LAYOUT_5_1, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_SampleRate) {
  AddInput(MakeTestBuffer(48000, kOutChannelLayout, 512));
  AddInput(MakeTestBuffer(44100, kOutChannelLayout, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_ChannelLayout) {
  AddInput(MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_STEREO, 512));
  AddInput(MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_MONO, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_SampleRateAndChannelLayout) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 512));
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_MONO, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_Multiple) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 512));
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_MONO, 512));
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_5_1, 512));
  AddInput(MakeTestBuffer(22050, CHANNEL_LAYOUT_STEREO, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Reset) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 512));
  Reset();
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResampleThenReset) {
  // Resampling is likely to leave some data buffered in AudioConverter's
  // fifo or resampler, so make sure Reset() cleans that all up.
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_STEREO, 512));
  Reset();
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResetThenConvert) {
  AddInput(MakeTestBuffer(kOutSampleRate, kOutChannelLayout, 512));
  Reset();
  // Make sure we can keep using the AudioBufferConverter after we've Reset().
  AddInput(MakeTestBuffer(kOutSampleRate, kOutChannelLayout, 512));
  ConsumeAllOutput();
}

}  // namespace media
