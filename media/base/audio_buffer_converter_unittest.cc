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

// Important: Use an odd buffer size here so SIMD issues are caught.
const int kOutFrameSize = 441;
const int kOutSampleRate = 44100;
const ChannelLayout kOutChannelLayout = CHANNEL_LAYOUT_STEREO;
const int kOutChannelCount = 2;

static scoped_refptr<AudioBuffer> MakeTestBuffer(int sample_rate,
                                                 ChannelLayout channel_layout,
                                                 int channel_count,
                                                 int frames) {
  return MakeAudioBuffer<uint8>(kSampleFormatU8,
                                channel_layout,
                                channel_count,
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
                                  kOutFrameSize);
    ResetConverter(output_params);
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
        EXPECT_EQ(out->sample_rate(), out_sample_rate_);
        EXPECT_EQ(out->channel_layout(), out_channel_layout_);
        EXPECT_EQ(out->channel_count(), out_channel_count_);
      } else {
        EXPECT_FALSE(audio_buffer_converter_->HasNextBuffer());
      }
    }
    EXPECT_EQ(output_frames_, ceil(expected_output_frames_));
  }

  void ResetConverter(AudioParameters out_params) {
    audio_buffer_converter_.reset(new AudioBufferConverter(out_params));
    out_channel_layout_ = out_params.channel_layout();
    out_channel_count_ = out_params.channels();
    out_sample_rate_ = out_params.sample_rate();
  }

 private:
  scoped_ptr<AudioBufferConverter> audio_buffer_converter_;

  int input_frames_;
  double expected_output_frames_;
  int output_frames_;

  int out_sample_rate_;
  ChannelLayout out_channel_layout_;
  int out_channel_count_;
};

TEST_F(AudioBufferConverterTest, PassThrough) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, kOutChannelLayout, kOutChannelCount, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Downsample) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(48000, kOutChannelLayout, kOutChannelCount, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Upsample) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(8000, kOutChannelLayout, kOutChannelCount, 512);
  AddInput(in);
  ConsumeAllOutput();
}

// Test resampling a buffer smaller than the SincResampler's kernel size.
TEST_F(AudioBufferConverterTest, Resample_TinyBuffer) {
  AddInput(MakeTestBuffer(
      48000, CHANNEL_LAYOUT_STEREO, 2, SincResampler::kKernelSize - 1));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Resample_DifferingBufferSizes) {
  const int input_sample_rate = 48000;
  AddInput(MakeTestBuffer(
      input_sample_rate, kOutChannelLayout, kOutChannelCount, 100));
  AddInput(MakeTestBuffer(
      input_sample_rate, kOutChannelLayout, kOutChannelCount, 200));
  AddInput(MakeTestBuffer(
      input_sample_rate, kOutChannelLayout, kOutChannelCount, 300));
  AddInput(MakeTestBuffer(
      input_sample_rate, kOutChannelLayout, kOutChannelCount, 400));
  AddInput(MakeTestBuffer(
      input_sample_rate, kOutChannelLayout, kOutChannelCount, 500));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ChannelDownmix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_MONO, 1, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ChannelUpmix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_5_1, 6, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResampleAndRemix) {
  scoped_refptr<AudioBuffer> in =
      MakeTestBuffer(48000, CHANNEL_LAYOUT_5_1, 6, 512);
  AddInput(in);
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_SampleRate) {
  AddInput(MakeTestBuffer(48000, kOutChannelLayout, kOutChannelCount, 512));
  AddInput(MakeTestBuffer(44100, kOutChannelLayout, kOutChannelCount, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_ChannelLayout) {
  AddInput(MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_STEREO, 2, 512));
  AddInput(MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_MONO, 1, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_SampleRateAndChannelLayout) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 2, 512));
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_MONO, 1, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ConfigChange_Multiple) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 2, 512));
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_MONO, 1, 512));
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_5_1, 6, 512));
  AddInput(MakeTestBuffer(22050, CHANNEL_LAYOUT_STEREO, 2, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, Reset) {
  AddInput(MakeTestBuffer(44100, CHANNEL_LAYOUT_STEREO, 2, 512));
  Reset();
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResampleThenReset) {
  // Resampling is likely to leave some data buffered in AudioConverter's
  // fifo or resampler, so make sure Reset() cleans that all up.
  AddInput(MakeTestBuffer(48000, CHANNEL_LAYOUT_STEREO, 2, 512));
  Reset();
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, ResetThenConvert) {
  AddInput(
      MakeTestBuffer(kOutSampleRate, kOutChannelLayout, kOutChannelCount, 512));
  Reset();
  // Make sure we can keep using the AudioBufferConverter after we've Reset().
  AddInput(
      MakeTestBuffer(kOutSampleRate, kOutChannelLayout, kOutChannelCount, 512));
  ConsumeAllOutput();
}

TEST_F(AudioBufferConverterTest, DiscreteChannelLayout) {
  AudioParameters output_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                CHANNEL_LAYOUT_DISCRETE,
                                2,
                                0,
                                kOutSampleRate,
                                16,
                                512,
                                0);
  ResetConverter(output_params);
  AddInput(MakeTestBuffer(kOutSampleRate, CHANNEL_LAYOUT_STEREO, 2, 512));
  ConsumeAllOutput();
}

}  // namespace media
