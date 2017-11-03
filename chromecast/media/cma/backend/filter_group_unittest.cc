// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/filter_group.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chromecast/media/cma/backend/post_processing_pipeline.h"
#include "chromecast/media/cma/backend/stream_mixer.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

using ::testing::_;

namespace {

// Total of Test samples including left and right channels.
#define NUM_SAMPLES 64

constexpr size_t kBytesPerSample = sizeof(int32_t);
constexpr int kNumInputChannels = 2;
constexpr int kInputSampleRate = 48000;
constexpr int kInputFrames = NUM_SAMPLES / 2;

class MockPostProcessor : public PostProcessingPipeline {
 public:
  MockPostProcessor() {}
  ~MockPostProcessor() override {}
  MOCK_METHOD4(
      ProcessFrames,
      int(float* data, int num_frames, float current_volume, bool is_silence));
  MOCK_METHOD2(SetPostProcessorConfig,
               void(const std::string& name, const std::string& config));

 private:
  bool SetSampleRate(int sample_rate) override { return true; }
  bool IsRinging() override { return false; }
  int delay() { return 0; }
  std::string name() const { return "mock"; }

  DISALLOW_COPY_AND_ASSIGN(MockPostProcessor);
};

// PostProcessor that inverts one channel.
class InvertChannelPostProcessor : public PostProcessingPipeline {
 public:
  explicit InvertChannelPostProcessor(int channels, int channel_to_invert)
      : channels_(channels), channel_to_invert_(channel_to_invert) {
    ON_CALL(*this, ProcessFrames(_, _, _, _))
        .WillByDefault(testing::Invoke(
            this, &InvertChannelPostProcessor::DoInvertChannel));
  }

  ~InvertChannelPostProcessor() override {}

  MOCK_METHOD4(
      ProcessFrames,
      int(float* data, int num_frames, float current_volume, bool is_silence));
  MOCK_METHOD2(SetPostProcessorConfig,
               void(const std::string& name, const std::string& config));

 private:
  int DoInvertChannel(float* data,
                      int num_frames,
                      float current_volume,
                      bool is_silence) {
    for (int fr = 0; fr < num_frames; ++fr) {
      for (int ch = 0; ch < channels_; ++ch) {
        if (ch == channel_to_invert_) {
          data[fr * channels_ + ch] *= -1;
        }
      }
    }
    return 0;
  }

  bool SetSampleRate(int sample_rate) override { return true; }
  bool IsRinging() override { return false; }
  int delay() { return 0; }
  std::string name() const { return "invert"; }

  int channels_;
  int channel_to_invert_;

  DISALLOW_COPY_AND_ASSIGN(InvertChannelPostProcessor);
};

}  // namespace

// Note: Test data should be represented as 32-bit integers and copied into
// ::media::AudioBus instances, rather than wrapping statically declared float
// arrays.
constexpr int32_t kTestData[NUM_SAMPLES] = {
    74343736,    -1333200799, -1360871126, 1138806283,  1931811865,
    1856308487,  649203634,   564640023,   1676630678,  23416591,
    -1293255456, 547928305,   -976258952,  1840550252,  1714525174,
    358704931,   983646295,   1264863573,  442473973,   1222979052,
    317404525,   366912613,   1393280948,  -1022004648, -2054669405,
    -159762261,  1127018745,  -1984491787, 1406988336,  -693327981,
    -1549544744, 1232236854,  970338338,   -1750160519, -783213057,
    1231504562,  1155296810,  -820018779,  1155689800,  -1108462340,
    -150535168,  1033717023,  2121241397,  1829995370,  -1893006836,
    -819097508,  -495186107,  1001768909,  -1441111852, 692174781,
    1916569026,  -687787473,  -910565280,  1695751872,  994166817,
    1775451433,  909418522,   492671403,   -761744663,  -2064315902,
    1357716471,  -1580019684, 1872702377,  -1524457840,
};

// Return a scoped pointer filled with the data above.
std::unique_ptr<::media::AudioBus> GetTestData() {
  int samples = NUM_SAMPLES / kNumInputChannels;
  auto data = ::media::AudioBus::Create(kNumInputChannels, samples);
  data->FromInterleaved(kTestData, samples, kBytesPerSample);
  return data;
}

// TODO(erickung): Consolidate this mock class with the one used in
// StreamMixerTest.
// Class to provide 64 fake audio samples.
class MockInputQueue : public StreamMixer::InputQueue {
 public:
  MockInputQueue() : data_(GetTestData()) {}
  ~MockInputQueue() override = default;

  const ::media::AudioBus* data() const { return data_.get(); }

 private:
  // StreamMixer::InputQueue implementation.
  int input_samples_per_second() const override { return kInputSampleRate; }
  bool primary() const override { return true; }
  std::string device_id() const override { return "test"; }
  AudioContentType content_type() const override {
    return AudioContentType::kMedia;
  }
  bool IsDeleting() const override { return false; }
  void Initialize(const MediaPipelineBackend::AudioDecoder::RenderingDelay&
                      mixer_rendering_delay) override {}
  void set_filter_group(FilterGroup* filter_group) override {
    filter_group_ = filter_group;
  }
  FilterGroup* filter_group() override { return filter_group_; }
  int MaxReadSize() override { return NUM_SAMPLES; }
  void GetResampledData(::media::AudioBus* dest, int frames) override {
    DCHECK(data_);
    data_->CopyPartialFramesTo(0, frames, 0, dest);
  }
  void VolumeScaleAccumulate(bool repeat_transition,
                             const float* src,
                             int frames,
                             float* dest) override {
    DCHECK(dest);
    DCHECK(src);
    // Copy the original audio data.
    std::memcpy(dest, src, frames * sizeof(float));
  }
  void OnSkipped() override {}
  void AfterWriteFrames(
      const MediaPipelineBackend::AudioDecoder::RenderingDelay&
          mixer_rendering_delay) override {}
  void SignalError(StreamMixerInput::MixerError error) override {}
  void PrepareToDelete(const OnReadyToDeleteCb& delete_cb) override {}
  void SetContentTypeVolume(float volume, int fade_ms) override {}
  void SetMuted(bool muted) override {}
  float EffectiveVolume() override { return 1.0; }
  float InstantaneousVolume() override { return 1.0; }

 private:
  FilterGroup* filter_group_ = nullptr;
  std::unique_ptr<::media::AudioBus> data_;

  DISALLOW_COPY_AND_ASSIGN(MockInputQueue);
};

class FilterGroupTest : public testing::Test {
 protected:
  FilterGroupTest() : input_(std::make_unique<MockInputQueue>()) {}
  ~FilterGroupTest() override {}

  void MakeFilterGroup(bool mix_to_mono,
                       std::unique_ptr<PostProcessingPipeline> post_processor) {
    filter_group_ = std::make_unique<FilterGroup>(
        kNumInputChannels, mix_to_mono, "test_filter",
        std::move(post_processor),
        std::unordered_set<std::string>() /* device_ids */,
        std::vector<FilterGroup*>());
    filter_group_->Initialize(kInputSampleRate);
    filter_group_->AddActiveInput(input_.get());
  }

  float Input(int channel, int frame) {
    DCHECK_LE(channel, input_->data()->channels());
    DCHECK_LE(frame, input_->data()->frames());
    return input_->data()->channel(channel)[frame];
  }

  float LeftInput(int frame) { return Input(0, frame); }

  float RightInput(int frame) { return Input(1, frame); }

  std::unique_ptr<MockInputQueue> input_;
  std::unique_ptr<FilterGroup> filter_group_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FilterGroupTest);
};

TEST_F(FilterGroupTest, Passthrough) {
  MakeFilterGroup(false /* mix to mono */,
                  std::make_unique<MockPostProcessor>());
  filter_group_->MixAndFilter(kInputFrames);

  // Verify if the fiter group output matches the source.
  float* interleaved_data = filter_group_->interleaved();
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      ASSERT_EQ(Input(ch, f), interleaved_data[f * kNumInputChannels + ch]);
    }
  }
}

TEST_F(FilterGroupTest, MonoMixer) {
  MakeFilterGroup(true /* mix to mono */,
                  std::make_unique<MockPostProcessor>());
  filter_group_->MixAndFilter(kInputFrames);

  // Verify if the fiter group output matches the source after down mixing.
  float* interleaved_data = filter_group_->interleaved();
  for (int i = 0; i < kInputFrames; ++i) {
    ASSERT_EQ((LeftInput(i) + RightInput(i)) / 2, interleaved_data[i * 2]);
    ASSERT_EQ(interleaved_data[i * 2], interleaved_data[i * 2 + 1]);
  }
}

TEST_F(FilterGroupTest, MonoMixesAfterPostProcessors) {
  MakeFilterGroup(
      true /* mix to mono */,
      std::make_unique<InvertChannelPostProcessor>(kNumInputChannels, 0));
  filter_group_->MixAndFilter(kInputFrames);

  // Verify both output channels = (-1 * ch0 + ch1) / 2 after mixing.
  // If order of mixing, filtering is incorrect, the channels won't match.
  float* interleaved_data = filter_group_->interleaved();
  for (int i = 0; i < kInputFrames; ++i) {
    ASSERT_EQ((-LeftInput(i) + RightInput(i)) / 2, interleaved_data[i * 2]);
    ASSERT_EQ(interleaved_data[i * 2], interleaved_data[i * 2 + 1]);
  }
}

TEST_F(FilterGroupTest, SelectsOutputChannel) {
  MakeFilterGroup(false /* mix to mono */,
                  std::make_unique<MockPostProcessor>());

  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames);

  float* interleaved_data = filter_group_->interleaved();
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Both output channels should be equal to left channel.
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], LeftInput(f));
    }
  }

  filter_group_->UpdatePlayoutChannel(1);
  filter_group_->MixAndFilter(kInputFrames);
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Both output channels should be equal to right channel.
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], RightInput(f));
    }
  }

  filter_group_->UpdatePlayoutChannel(-1);
  filter_group_->MixAndFilter(kInputFrames);
  for (int f = 0; f < kInputFrames; ++f) {
    for (int ch = 0; ch < kNumInputChannels; ++ch) {
      // Back to normal (passthrough).
      ASSERT_EQ(interleaved_data[f * kNumInputChannels + ch], Input(ch, f));
    }
  }
}

TEST_F(FilterGroupTest, SelectsOutputChannelBeforePostProcessors) {
  MakeFilterGroup(
      false /* mix to mono */,
      std::make_unique<InvertChannelPostProcessor>(kNumInputChannels, 0));
  filter_group_->UpdatePlayoutChannel(0);
  filter_group_->MixAndFilter(kInputFrames);

  float* interleaved_data = filter_group_->interleaved();
  for (int f = 0; f < kInputFrames; ++f) {
    // channel 0 out = channel 0 in * -1
    // channel 1 out = channel 0 in
    // (If order is wrong, both channels will be channel_0_in * -1).
    ASSERT_EQ(interleaved_data[f * kNumInputChannels], LeftInput(f) * -1);
    ASSERT_EQ(interleaved_data[f * kNumInputChannels + 1], LeftInput(f));
  }
}

}  // namespace media
}  // namespace chromecast
