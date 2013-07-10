// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The format of these tests are to enqueue a known amount of data and then
// request the exact amount we expect in order to dequeue the known amount of
// data.  This ensures that for any rate we are consuming input data at the
// correct rate.  We always pass in a very large destination buffer with the
// expectation that FillBuffer() will fill as much as it can but no more.

#include <cmath>

#include "base/bind.h"
#include "base/callback.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/buffers.h"
#include "media/base/channel_layout.h"
#include "media/base/test_helpers.h"
#include "media/filters/audio_renderer_algorithm.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const int kFrameSize = 250;
static const int kSamplesPerSecond = 3000;
static const SampleFormat kSampleFormat = kSampleFormatS16;

class AudioRendererAlgorithmTest : public testing::Test {
 public:
  AudioRendererAlgorithmTest()
      : frames_enqueued_(0),
        channels_(0),
        sample_format_(kUnknownSampleFormat),
        bytes_per_sample_(0) {
  }

  virtual ~AudioRendererAlgorithmTest() {}

  void Initialize() {
    Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, 3000);
  }

  void Initialize(ChannelLayout channel_layout,
                  SampleFormat sample_format,
                  int samples_per_second) {
    channels_ = ChannelLayoutToChannelCount(channel_layout);
    sample_format_ = sample_format;
    bytes_per_sample_ = SampleFormatToBytesPerChannel(sample_format);
    AudioParameters params(media::AudioParameters::AUDIO_PCM_LINEAR,
                           channel_layout,
                           samples_per_second,
                           bytes_per_sample_ * 8,
                           samples_per_second / 100);
    algorithm_.Initialize(1, params);
    FillAlgorithmQueue();
  }

  void FillAlgorithmQueue() {
    // The value of the data is meaningless; we just want non-zero data to
    // differentiate it from muted data.
    scoped_refptr<AudioBuffer> buffer;
    while (!algorithm_.IsQueueFull()) {
      switch (sample_format_) {
        case kSampleFormatU8:
          buffer = MakeInterleavedAudioBuffer<uint8>(sample_format_,
                                                     channels_,
                                                     1,
                                                     1,
                                                     kFrameSize,
                                                     kNoTimestamp(),
                                                     kNoTimestamp());
          break;
        case kSampleFormatS16:
          buffer = MakeInterleavedAudioBuffer<int16>(sample_format_,
                                                     channels_,
                                                     1,
                                                     1,
                                                     kFrameSize,
                                                     kNoTimestamp(),
                                                     kNoTimestamp());
          break;
        case kSampleFormatS32:
          buffer = MakeInterleavedAudioBuffer<int32>(sample_format_,
                                                     channels_,
                                                     1,
                                                     1,
                                                     kFrameSize,
                                                     kNoTimestamp(),
                                                     kNoTimestamp());
          break;
        default:
          NOTREACHED() << "Unrecognized format " << sample_format_;
      }
      algorithm_.EnqueueBuffer(buffer);
      frames_enqueued_ += kFrameSize;
    }
  }

  void CheckFakeData(AudioBus* audio_data, int frames_written) {
    // Check each channel individually.
    for (int ch = 0; ch < channels_; ++ch) {
      bool all_zero = true;
      for (int i = 0; i < frames_written && all_zero; ++i)
        all_zero = audio_data->channel(ch)[i] == 0.0f;
      ASSERT_EQ(algorithm_.is_muted(), all_zero) << " for channel " << ch;
    }
  }

  int ComputeConsumedFrames(int initial_frames_enqueued,
                            int initial_frames_buffered) {
    int frame_delta = frames_enqueued_ - initial_frames_enqueued;
    int buffered_delta = algorithm_.frames_buffered() - initial_frames_buffered;
    int consumed = frame_delta - buffered_delta;
    CHECK_GE(consumed, 0);
    return consumed;
  }

  void TestPlaybackRate(double playback_rate) {
    const int kDefaultBufferSize = algorithm_.samples_per_second() / 100;
    const int kDefaultFramesRequested = 2 * algorithm_.samples_per_second();

    TestPlaybackRate(
        playback_rate, kDefaultBufferSize, kDefaultFramesRequested);
  }

  void TestPlaybackRate(double playback_rate,
                        int buffer_size_in_frames,
                        int total_frames_requested) {
    int initial_frames_enqueued = frames_enqueued_;
    int initial_frames_buffered = algorithm_.frames_buffered();
    algorithm_.SetPlaybackRate(static_cast<float>(playback_rate));

    scoped_ptr<AudioBus> bus =
        AudioBus::Create(channels_, buffer_size_in_frames);
    if (playback_rate == 0.0) {
      int frames_written =
          algorithm_.FillBuffer(bus.get(), buffer_size_in_frames);
      EXPECT_EQ(0, frames_written);
      return;
    }

    int frames_remaining = total_frames_requested;
    while (frames_remaining > 0) {
      int frames_requested = std::min(buffer_size_in_frames, frames_remaining);
      int frames_written = algorithm_.FillBuffer(bus.get(), frames_requested);
      ASSERT_GT(frames_written, 0) << "Requested: " << frames_requested
                                   << ", playing at " << playback_rate;
      CheckFakeData(bus.get(), frames_written);
      frames_remaining -= frames_written;

      FillAlgorithmQueue();
    }

    int frames_consumed =
        ComputeConsumedFrames(initial_frames_enqueued, initial_frames_buffered);

    // If playing back at normal speed, we should always get back the same
    // number of bytes requested.
    if (playback_rate == 1.0) {
      EXPECT_EQ(total_frames_requested, frames_consumed);
      return;
    }

    // Otherwise, allow |kMaxAcceptableDelta| difference between the target and
    // actual playback rate.
    // When |kSamplesPerSecond| and |total_frames_requested| are reasonably
    // large, one can expect less than a 1% difference in most cases. In our
    // current implementation, sped up playback is less accurate than slowed
    // down playback, and for playback_rate > 1, playback rate generally gets
    // less and less accurate the farther it drifts from 1 (though this is
    // nonlinear).
    double actual_playback_rate =
        1.0 * frames_consumed / total_frames_requested;
    EXPECT_NEAR(playback_rate, actual_playback_rate, playback_rate / 100.0);
  }

 protected:
  AudioRendererAlgorithm algorithm_;
  int frames_enqueued_;
  int channels_;
  SampleFormat sample_format_;
  int bytes_per_sample_;
};

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NormalRate) {
  Initialize();
  TestPlaybackRate(1.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalFasterRate) {
  Initialize();
  TestPlaybackRate(1.0001);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_NearlyNormalSlowerRate) {
  Initialize();
  TestPlaybackRate(0.9999);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAQuarterRate) {
  Initialize();
  TestPlaybackRate(1.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_OneAndAHalfRate) {
  Initialize();
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_DoubleRate) {
  Initialize();
  TestPlaybackRate(2.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_EightTimesRate) {
  Initialize();
  TestPlaybackRate(8.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_ThreeQuartersRate) {
  Initialize();
  TestPlaybackRate(0.75);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HalfRate) {
  Initialize();
  TestPlaybackRate(0.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_QuarterRate) {
  Initialize();
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_Pause) {
  Initialize();
  TestPlaybackRate(0.0);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SlowDown) {
  Initialize();
  TestPlaybackRate(4.5);
  TestPlaybackRate(3.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(0.25);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SpeedUp) {
  Initialize();
  TestPlaybackRate(0.25);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.0);
  TestPlaybackRate(2.0);
  TestPlaybackRate(3.0);
  TestPlaybackRate(4.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_JumpAroundSpeeds) {
  Initialize();
  TestPlaybackRate(2.1);
  TestPlaybackRate(0.9);
  TestPlaybackRate(0.6);
  TestPlaybackRate(1.4);
  TestPlaybackRate(0.3);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_SmallBufferSize) {
  Initialize();
  static const int kBufferSizeInFrames = 1;
  static const int kFramesRequested = 2 * kSamplesPerSecond;
  TestPlaybackRate(1.0, kBufferSizeInFrames, kFramesRequested);
  TestPlaybackRate(0.5, kBufferSizeInFrames, kFramesRequested);
  TestPlaybackRate(1.5, kBufferSizeInFrames, kFramesRequested);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LargeBufferSize) {
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS16, 44100);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_LowerQualityAudio) {
  Initialize(CHANNEL_LAYOUT_MONO, kSampleFormatU8, kSamplesPerSecond);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

TEST_F(AudioRendererAlgorithmTest, FillBuffer_HigherQualityAudio) {
  Initialize(CHANNEL_LAYOUT_STEREO, kSampleFormatS32, kSamplesPerSecond);
  TestPlaybackRate(1.0);
  TestPlaybackRate(0.5);
  TestPlaybackRate(1.5);
}

}  // namespace media
