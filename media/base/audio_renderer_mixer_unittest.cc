// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MSVC++ requires this to be set before any other includes to get M_PI.
#define _USE_MATH_DEFINES
#include <cmath>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Parameters which control the many input case tests.
static const int kMixerInputs = 8;
static const int kMixerCycles = 3;

// Parameters used for testing.
static const int kBitsPerChannel = 16;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kHighLatencyBufferSize = 8192;
static const int kLowLatencyBufferSize = 256;

// Number of full sine wave cycles for each Render() call.
static const int kSineCycles = 4;

// Tuple of <input sampling rate, output sampling rate, epsilon>.
typedef std::tr1::tuple<int, int, double> AudioRendererMixerTestData;
class AudioRendererMixerTest
    : public testing::TestWithParam<AudioRendererMixerTestData> {
 public:
  AudioRendererMixerTest()
      : epsilon_(std::tr1::get<2>(GetParam())),
        half_fill_(false) {
    // Create input and output parameters based on test parameters.
    input_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
        std::tr1::get<0>(GetParam()), kBitsPerChannel, kHighLatencyBufferSize);
    output_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, kChannelLayout,
        std::tr1::get<1>(GetParam()), 16, kLowLatencyBufferSize);

    sink_ = new MockAudioRendererSink();
    EXPECT_CALL(*sink_, Start());
    EXPECT_CALL(*sink_, Stop());

    mixer_.reset(new AudioRendererMixer(
        input_parameters_, output_parameters_, sink_));
    mixer_callback_ = sink_->callback();

    // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
    // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
    audio_data_.reserve(output_parameters_.channels());
    for (int i = 0; i < output_parameters_.channels(); ++i)
      audio_data_.push_back(new float[output_parameters_.frames_per_buffer()]);

    // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
    // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
    expected_audio_data_.reserve(output_parameters_.channels());
    for (int i = 0; i < output_parameters_.channels(); ++i) {
      expected_audio_data_.push_back(
          new float[output_parameters_.frames_per_buffer()]);
    }

    // Allocate one callback for generating expected results.
    double step = kSineCycles / static_cast<double>(
        output_parameters_.frames_per_buffer());
    expected_callback_.reset(new FakeAudioRenderCallback(step));
  }

  AudioRendererMixer* GetMixer(const AudioParameters& params) {
    return mixer_.get();
  }

  MOCK_METHOD1(RemoveMixer, void(const AudioParameters&));

  void InitializeInputs(int count) {
    mixer_inputs_.reserve(count);
    fake_callbacks_.reserve(count);

    // Setup FakeAudioRenderCallback step to compensate for resampling.
    double scale_factor = input_parameters_.sample_rate()
        / static_cast<double>(output_parameters_.sample_rate());
    double step = kSineCycles / (scale_factor *
        static_cast<double>(output_parameters_.frames_per_buffer()));

    for (int i = 0; i < count; ++i) {
      fake_callbacks_.push_back(new FakeAudioRenderCallback(step));
      mixer_inputs_.push_back(new AudioRendererMixerInput(
          base::Bind(&AudioRendererMixerTest::GetMixer,
                     base::Unretained(this)),
          base::Bind(&AudioRendererMixerTest::RemoveMixer,
                     base::Unretained(this))));
      mixer_inputs_[i]->Initialize(input_parameters_, fake_callbacks_[i]);
      mixer_inputs_[i]->SetVolume(1.0f);
    }
    EXPECT_CALL(*this, RemoveMixer(testing::_)).Times(count);
  }

  bool ValidateAudioData(int index, int frames, float scale) {
    for (size_t i = 0; i < audio_data_.size(); ++i) {
      for (int j = index; j < frames; j++) {
        double error = fabs(
            audio_data_[i][j] - expected_audio_data_[i][j] * scale);
        if (error > epsilon_) {
          EXPECT_NEAR(
              expected_audio_data_[i][j] * scale, audio_data_[i][j], epsilon_)
              << " i=" << i << ", j=" << j;
          return false;
        }
      }
    }
    return true;
  }

  bool RenderAndValidateAudioData(float scale) {
    int request_frames = output_parameters_.frames_per_buffer();

    // Half fill won't be exactly half when resampling since the resampler
    // will have enough data to fill out more of the buffer based on its
    // internal buffer and kernel size.  So special case some of the checks.
    bool resampling = input_parameters_.sample_rate()
        != output_parameters_.sample_rate();

    if (half_fill_) {
      for (size_t i = 0; i < fake_callbacks_.size(); ++i)
        fake_callbacks_[i]->set_half_fill(true);
      expected_callback_->set_half_fill(true);
      for (size_t i = 0; i < expected_audio_data_.size(); ++i) {
        memset(expected_audio_data_[i], 0,
               sizeof(*expected_audio_data_[i]) * request_frames);
      }
    }

    // Render actual audio data.
    int frames = mixer_callback_->Render(audio_data_, request_frames, 0);
    if (frames != request_frames)
      return false;

    // Render expected audio data (without scaling).
    expected_callback_->Render(expected_audio_data_, request_frames, 0);

    if (half_fill_) {
      // Verify first half of audio data for both resampling and non-resampling.
      if (!ValidateAudioData(0, frames / 2, scale))
        return false;
      // Verify silence in the second half if we're not resampling.
      if (!resampling)
        return ValidateAudioData(frames / 2, frames, 0);
      return true;
    } else {
      return ValidateAudioData(0, frames, scale);
    }
  }

  // Fill |audio_data_| fully with |value|.
  void FillAudioData(float value) {
    for (size_t i = 0; i < audio_data_.size(); ++i)
      std::fill(audio_data_[i],
                audio_data_[i] + output_parameters_.frames_per_buffer(), value);
  }

  // Verify silence when mixer inputs are in pre-Start() and post-Start().
  void StartTest(int inputs) {
    InitializeInputs(inputs);

    // Verify silence before any inputs have been started.  Fill the buffer
    // before hand with non-zero data to ensure we get zeros back.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all even numbered mixer inputs and ensure we still get silence.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Start();
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    // Start() all mixer inputs and ensure we still get silence.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Start();
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in post-Play() state.
  void PlayTest(int inputs) {
    InitializeInputs(inputs);

    // Play() all mixer inputs and ensure we get the right values.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify volume adjusted output when mixer inputs are in post-Play() state.
  void PlayVolumeAdjustedTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Set a different volume for each mixer input and verify the results.
    float total_scale = 0;
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      float volume = static_cast<float>(i) / mixer_inputs_.size();
      total_scale += volume;
      EXPECT_TRUE(mixer_inputs_[i]->SetVolume(volume));
    }
    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(total_scale));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs can only partially fulfill a Render().
  void PlayPartialRenderTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Verify a properly filled buffer when half filled (remainder zeroed).
    half_fill_ = true;
    ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size()));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in Pause() state.
  void PauseTest(int inputs) {
    InitializeInputs(inputs);

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Pause() all even numbered mixer inputs and ensure we get the right value.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Pause(false);
    for (int i = 0; i < kMixerCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(mixer_inputs_.size() / 2));

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs are in post-Stop() state.
  void StopTest(int inputs) {
    InitializeInputs(inputs);

    // Start() and Stop() all inputs.
    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Stop();
    }

    // Verify we get silence back; fill |audio_data_| before hand to be sure.
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
  }

 protected:
  virtual ~AudioRendererMixerTest() {
    for (size_t i = 0; i < audio_data_.size(); ++i)
      delete [] audio_data_[i];
    for (size_t i = 0; i < expected_audio_data_.size(); ++i)
      delete [] expected_audio_data_[i];
  }

  scoped_refptr<MockAudioRendererSink> sink_;
  scoped_ptr<AudioRendererMixer> mixer_;
  AudioRendererSink::RenderCallback* mixer_callback_;
  AudioParameters input_parameters_;
  AudioParameters output_parameters_;
  std::vector<float*> audio_data_;
  std::vector<float*> expected_audio_data_;
  std::vector< scoped_refptr<AudioRendererMixerInput> > mixer_inputs_;
  ScopedVector<FakeAudioRenderCallback> fake_callbacks_;
  scoped_ptr<FakeAudioRenderCallback> expected_callback_;
  double epsilon_;
  bool half_fill_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerTest);
};

// Verify a mixer with no inputs returns silence for all requested frames.
TEST_P(AudioRendererMixerTest, NoInputs) {
  FillAudioData(1.0f);
  EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
}

// Test mixer output with one input in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, OneInputStart) {
  StartTest(1);
}

// Test mixer output with many inputs in the pre-Start() and post-Start() state.
TEST_P(AudioRendererMixerTest, ManyInputStart) {
  StartTest(kMixerInputs);
}

// Test mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlay) {
  PlayTest(1);
}

// Test mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlay) {
  PlayTest(kMixerInputs);
}

// Test volume adjusted mixer output with one input in the post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(1);
}

// Test volume adjusted mixer output with many inputs in the post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(kMixerInputs);
}

// Test mixer output with one input and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, OneInputPlayPartialRender) {
  PlayPartialRenderTest(1);
}

// Test mixer output with many inputs and partial Render() in post-Play() state.
TEST_P(AudioRendererMixerTest, ManyInputPlayPartialRender) {
  PlayPartialRenderTest(kMixerInputs);
}

// Test mixer output with one input in the post-Pause() state.
TEST_P(AudioRendererMixerTest, OneInputPause) {
  PauseTest(1);
}

// Test mixer output with many inputs in the post-Pause() state.
TEST_P(AudioRendererMixerTest, ManyInputPause) {
  PauseTest(kMixerInputs);
}

// Test mixer output with one input in the post-Stop() state.
TEST_P(AudioRendererMixerTest, OneInputStop) {
  StopTest(1);
}

// Test mixer output with many inputs in the post-Stop() state.
TEST_P(AudioRendererMixerTest, ManyInputStop) {
  StopTest(kMixerInputs);
}

// Test mixer with many inputs in mixed post-Stop() and post-Play() states.
TEST_P(AudioRendererMixerTest, ManyInputMixedStopPlay) {
  InitializeInputs(kMixerInputs);

  // Start() all inputs.
  for (size_t i = 0; i < mixer_inputs_.size(); ++i)
    mixer_inputs_[i]->Start();

  // Stop() all even numbered mixer inputs and Play() all odd numbered inputs
  // and ensure we get the right value.
  for (size_t i = 1; i < mixer_inputs_.size(); i += 2) {
    mixer_inputs_[i - 1]->Stop();
    mixer_inputs_[i]->Play();
  }
  ASSERT_TRUE(RenderAndValidateAudioData(std::max(
      mixer_inputs_.size() / 2, static_cast<size_t>(1))));

  for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
    mixer_inputs_[i]->Stop();
}

TEST_P(AudioRendererMixerTest, OnRenderError) {
  InitializeInputs(kMixerInputs);
  for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
    mixer_inputs_[i]->Start();
    EXPECT_CALL(*fake_callbacks_[i], OnRenderError()).Times(1);
  }

  mixer_callback_->OnRenderError();
  for (size_t i = 0; i < mixer_inputs_.size(); ++i)
    mixer_inputs_[i]->Stop();
}

INSTANTIATE_TEST_CASE_P(
    AudioRendererMixerTest, AudioRendererMixerTest, testing::Values(
        // No resampling.
        std::tr1::make_tuple(44100, 44100, 0.000000477),

        // Upsampling.
        std::tr1::make_tuple(44100, 48000, 0.0329405),

        // Downsampling.
        std::tr1::make_tuple(48000, 41000, 0.0410239)));

}  // namespace media
