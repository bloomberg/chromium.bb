// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "media/audio/audio_util.h"
#include "media/base/audio_renderer_mixer.h"
#include "media/base/audio_renderer_mixer_input.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Parameters which control the many input case tests.
static const int kMixerInputs = 64;
static const int kMixerCycles = 32;

static const int kBitsPerChannel = 16;
static const int kSampleRate = 48000;
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;

// Multiple rounds of addition result in precision loss with float values, so we
// need an epsilon such that if for all x f(x) = sum(x, m) and g(x) = m * x then
// fabs(f - g) < kEpsilon.  The kEpsilon below has been tested with m < 128.
static const float kEpsilon = 0.00015f;

class MockAudioRendererSink : public AudioRendererSink {
 public:
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD1(Pause, void(bool flush));
  MOCK_METHOD0(Play, void());
  MOCK_METHOD1(SetPlaybackRate, void(float rate));
  MOCK_METHOD1(SetVolume, bool(double volume));
  MOCK_METHOD1(GetVolume, void(double* volume));

  void Initialize(const media::AudioParameters& params,
                  AudioRendererSink::RenderCallback* renderer) OVERRIDE {
    // TODO(dalecurtis): Once we have resampling we need to ensure we are given
    // an AudioParameters which reflects the hardware settings.
    callback_ = renderer;
  };

  AudioRendererSink::RenderCallback* callback() {
    return callback_;
  }

  void SimulateRenderError() {
    callback_->OnRenderError();
  }

 protected:
  virtual ~MockAudioRendererSink() {}

  AudioRendererSink::RenderCallback* callback_;
};

class AudioRendererMixerTest : public ::testing::Test {
 public:
  AudioRendererMixerTest() {
    audio_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout, kSampleRate,
        kBitsPerChannel, GetHighLatencyOutputBufferSize(kSampleRate));
    sink_ = new MockAudioRendererSink();
    EXPECT_CALL(*sink_, Start());
    EXPECT_CALL(*sink_, Stop());

    mixer_ = new AudioRendererMixer(audio_parameters_, sink_);
    mixer_callback_ = sink_->callback();

    // TODO(dalecurtis): If we switch to AVX/SSE optimization, we'll need to
    // allocate these on 32-byte boundaries and ensure they're sized % 32 bytes.
    audio_data_.reserve(audio_parameters_.channels());
    for (int i = 0; i < audio_parameters_.channels(); ++i)
      audio_data_.push_back(new float[audio_parameters_.frames_per_buffer()]);

    fake_callback_.reset(new FakeAudioRenderCallback(audio_parameters_));
  }

  void InitializeInputs(int count) {
    for (int i = 0; i < count; ++i) {
      scoped_refptr<AudioRendererMixerInput> mixer_input(
          new AudioRendererMixerInput(mixer_));
      mixer_input->Initialize(audio_parameters_, fake_callback_.get());
      mixer_input->SetVolume(1.0f);
      mixer_inputs_.push_back(mixer_input);
    }
  }

  bool ValidateAudioData(int start_index, int frames, float check_value) {
    for (size_t i = 0; i < audio_data_.size(); ++i) {
      for (int j = start_index; j < frames; j++) {
        if (fabs(audio_data_[i][j] - check_value) > kEpsilon) {
          EXPECT_NEAR(check_value, audio_data_[i][j], kEpsilon)
              << " i=" << i << ", j=" << j;
          return false;
        }
      }
    }
    return true;
  }

  // Render audio_parameters_.frames_per_buffer() frames into |audio_data_| and
  // verify the result against |check_value|.
  bool RenderAndValidateAudioData(float check_value) {
    int frames = mixer_callback_->Render(
        audio_data_, audio_parameters_.frames_per_buffer(), 0);
    return frames == audio_parameters_.frames_per_buffer() && ValidateAudioData(
        0, audio_parameters_.frames_per_buffer(), check_value);
  }

  // Fill |audio_data_| fully with |value|.
  void FillAudioData(float value) {
    for (size_t i = 0; i < audio_data_.size(); ++i)
      std::fill(audio_data_[i],
                audio_data_[i] + audio_parameters_.frames_per_buffer(), value);
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

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Start();

    // Play() all even numbered mixer inputs and ensure we get the right value.
    for (size_t i = 0; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Play();
    for (int i = 0; i < kMixerCycles; ++i) {
      fake_callback_->NextFillValue();
      ASSERT_TRUE(RenderAndValidateAudioData(
          fake_callback_->fill_value() * std::max(
              mixer_inputs_.size() / 2, static_cast<size_t>(1))));
    }

    // Play() all mixer inputs and ensure we still get the right values.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Play();
    for (int i = 0; i < kMixerCycles; ++i) {
      fake_callback_->NextFillValue();
      ASSERT_TRUE(RenderAndValidateAudioData(
          fake_callback_->fill_value() * mixer_inputs_.size()));
    }

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
    for (int i = 0; i < kMixerCycles; ++i) {
      fake_callback_->NextFillValue();
      ASSERT_TRUE(RenderAndValidateAudioData(
          fake_callback_->fill_value() * total_scale));
    }

    for (size_t i = 0; i < mixer_inputs_.size(); ++i)
      mixer_inputs_[i]->Stop();
  }

  // Verify output when mixer inputs can only partially fulfill a Render().
  void PlayPartialRenderTest(int inputs) {
    InitializeInputs(inputs);
    int frames = audio_parameters_.frames_per_buffer();

    for (size_t i = 0; i < mixer_inputs_.size(); ++i) {
      mixer_inputs_[i]->Start();
      mixer_inputs_[i]->Play();
    }

    // Verify a properly filled buffer when half filled (remainder zeroed).
    fake_callback_->set_half_fill(true);
    for (int i = 0; i < kMixerCycles; ++i) {
      fake_callback_->NextFillValue();
      ASSERT_EQ(mixer_callback_->Render(audio_data_, frames, 0), frames);
      ASSERT_TRUE(ValidateAudioData(
          0, frames / 2, fake_callback_->fill_value() * mixer_inputs_.size()));
      ASSERT_TRUE(ValidateAudioData(
          frames / 2, frames, 0.0f));
    }
    fake_callback_->set_half_fill(false);

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
    for (int i = 0; i < kMixerCycles; ++i) {
      fake_callback_->NextFillValue();
      ASSERT_TRUE(RenderAndValidateAudioData(
          fake_callback_->fill_value() * (mixer_inputs_.size() / 2)));
    }

    // Pause() all the inputs and verify we get silence back.
    for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
      mixer_inputs_[i]->Pause(false);
    FillAudioData(1.0f);
    EXPECT_TRUE(RenderAndValidateAudioData(0.0f));

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
  }

  scoped_refptr<MockAudioRendererSink> sink_;
  scoped_refptr<AudioRendererMixer> mixer_;
  AudioRendererSink::RenderCallback* mixer_callback_;
  scoped_ptr<FakeAudioRenderCallback> fake_callback_;
  AudioParameters audio_parameters_;
  std::vector<float*> audio_data_;
  std::vector< scoped_refptr<AudioRendererMixerInput> > mixer_inputs_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererMixerTest);
};

// Verify a mixer with no inputs returns silence for all requested frames.
TEST_F(AudioRendererMixerTest, NoInputs) {
  FillAudioData(1.0f);
  EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
}

// Test mixer output with one input in the pre-Start() and post-Start() state.
TEST_F(AudioRendererMixerTest, OneInputStart) {
  StartTest(1);
}

// Test mixer output with many inputs in the pre-Start() and post-Start() state.
TEST_F(AudioRendererMixerTest, ManyInputStart) {
  StartTest(kMixerInputs);
}

// Test mixer output with one input in the post-Play() state.
TEST_F(AudioRendererMixerTest, OneInputPlay) {
  PlayTest(1);
}

// Test mixer output with many inputs in the post-Play() state.
TEST_F(AudioRendererMixerTest, ManyInputPlay) {
  PlayTest(kMixerInputs);
}

// Test volume adjusted mixer output with one input in the post-Play() state.
TEST_F(AudioRendererMixerTest, OneInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(1);
}

// Test volume adjusted mixer output with many inputs in the post-Play() state.
TEST_F(AudioRendererMixerTest, ManyInputPlayVolumeAdjusted) {
  PlayVolumeAdjustedTest(kMixerInputs);
}

// Test mixer output with one input and partial Render() in post-Play() state.
TEST_F(AudioRendererMixerTest, OneInputPlayPartialRender) {
  PlayPartialRenderTest(1);
}

// Test mixer output with many inputs and partial Render() in post-Play() state.
TEST_F(AudioRendererMixerTest, ManyInputPlayPartialRender) {
  PlayPartialRenderTest(kMixerInputs);
}

// Test mixer output with one input in the post-Pause() state.
TEST_F(AudioRendererMixerTest, OneInputPause) {
  PauseTest(1);
}

// Test mixer output with many inputs in the post-Pause() state.
TEST_F(AudioRendererMixerTest, ManyInputPause) {
  PauseTest(kMixerInputs);
}

// Test mixer output with one input in the post-Stop() state.
TEST_F(AudioRendererMixerTest, OneInputStop) {
  StopTest(1);
}

// Test mixer output with many inputs in the post-Stop() state.
TEST_F(AudioRendererMixerTest, ManyInputStop) {
  StopTest(kMixerInputs);
}

// Test mixer with many inputs in mixed post-Stop() and post-Play() states.
TEST_F(AudioRendererMixerTest, ManyInputMixedStopPlay) {
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
  for (int i = 0; i < kMixerCycles; ++i) {
    fake_callback_->NextFillValue();
    ASSERT_TRUE(RenderAndValidateAudioData(
        fake_callback_->fill_value() * std::max(
            mixer_inputs_.size() / 2, static_cast<size_t>(1))));
  }

  for (size_t i = 1; i < mixer_inputs_.size(); i += 2)
    mixer_inputs_[i]->Stop();
}

TEST_F(AudioRendererMixerTest, OnRenderError) {
  std::vector< scoped_refptr<AudioRendererMixerInput> > mixer_inputs;
  for (int i = 0; i < kMixerInputs; ++i) {
    scoped_refptr<AudioRendererMixerInput> mixer_input(
        new AudioRendererMixerInput(mixer_));
    mixer_input->Initialize(audio_parameters_, fake_callback_.get());
    mixer_input->SetVolume(1.0f);
    mixer_input->Start();
    mixer_inputs_.push_back(mixer_input);
  }

  EXPECT_CALL(*fake_callback_, OnRenderError()).Times(kMixerInputs);
  sink_->SimulateRenderError();
  for (int i = 0; i < kMixerInputs; ++i)
    mixer_inputs_[i]->Stop();
}

}  // namespace media
