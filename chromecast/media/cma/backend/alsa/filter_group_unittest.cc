// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/filter_group.h"

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {

// Total of Test damples including left and right channels.
#define NUM_SAMPLES 64

constexpr size_t kBytesPerSample = sizeof(int32_t);
constexpr int kNumInputChannels = 2;
constexpr int kInputSampleRate = 48000;

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
// StreamMixerAlsaTest.
// Class to provide a fake 64 audio samples.
class MockInputQueue : public StreamMixerAlsa::InputQueue {
 public:
  MockInputQueue() : data_(GetTestData()) {}
  ~MockInputQueue() override = default;

  // StreamMixerAlsa::InputQueue implementations. Most of them are dummy except
  // for
  int input_samples_per_second() const override { return kInputSampleRate; }
  bool primary() const override { return true; }
  std::string device_id() const override { return "test"; }
  AudioContentType content_type() const override {
    return AudioContentType::kMedia;
  }
  bool IsDeleting() const override { return false; }
  void Initialize(const MediaPipelineBackendAlsa::RenderingDelay&
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
    // Bypass the original audio data.
    std::memcpy(dest, src, frames * sizeof(float));
  }
  void OnSkipped() override {}
  void AfterWriteFrames(const MediaPipelineBackendAlsa::RenderingDelay&
                            mixer_rendering_delay) override {}
  void SignalError(StreamMixerAlsaInput::MixerError error) override {}
  void PrepareToDelete(const OnReadyToDeleteCb& delete_cb) override {}
  void SetContentTypeVolume(float volume, int fade_ms) override {}
  void SetMuted(bool muted) override {}
  float EffectiveVolume() override { return 1.0; }

  const ::media::AudioBus* data() const { return data_.get(); }

 private:
  FilterGroup* filter_group_ = nullptr;
  std::unique_ptr<::media::AudioBus> data_;

  DISALLOW_COPY_AND_ASSIGN(MockInputQueue);
};

class FilterGroupTest : public testing::Test {
 protected:
  FilterGroupTest() : message_loop_(new base::MessageLoop()) {}
  ~FilterGroupTest() override {}

 private:
  const std::unique_ptr<base::MessageLoop> message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FilterGroupTest);
};

TEST_F(FilterGroupTest, Passthrough) {
  const int num_output_channel = 2;
  std::unique_ptr<MockInputQueue> input(new MockInputQueue());
  base::ListValue empty_filter_list;
  std::unordered_set<std::string> empty_device_ids;
  std::unique_ptr<FilterGroup> filter_group(new FilterGroup(
      kNumInputChannels, false /* mix to mono */, "pass_through_filter",
      &empty_filter_list, empty_device_ids, std::vector<FilterGroup*>()));
  input->set_filter_group(filter_group.get());
  const int input_samples = NUM_SAMPLES / kNumInputChannels;
  filter_group->Initialize(kInputSampleRate);

  // Adds input queue into filter group.
  filter_group->AddActiveInput(input.get());

  // Mixing input in the filter group
  filter_group->MixAndFilter(input_samples);

  // Verify if the fiter group output matches the source.
  float* interleaved_data = filter_group->interleaved();
  const int output_sample_size = input_samples * num_output_channel;
  for (int i = 0; i < output_sample_size; ++i) {
    int channel = (i % 2 == 0) ? 0 : 1;
    float sample = input->data()->channel(channel)[i >> 1];
    ASSERT_EQ(sample, interleaved_data[i]);
  }
}

TEST_F(FilterGroupTest, MonoMixer) {
  const int num_output_channel = 1;
  const int input_samples = NUM_SAMPLES / kNumInputChannels;
  std::unique_ptr<MockInputQueue> input(new MockInputQueue());
  base::ListValue empty_filter_list;
  std::unordered_set<std::string> empty_device_ids;
  std::unique_ptr<FilterGroup> filter_group(new FilterGroup(
      kNumInputChannels, true /* mix to mono */, "mono_mix_filter",
      &empty_filter_list, empty_device_ids, std::vector<FilterGroup*>()));
  input->set_filter_group(filter_group.get());
  filter_group->Initialize(kInputSampleRate);

  // Adds input queue into filter group.
  filter_group->AddActiveInput(input.get());

  // Mixing input in the filter group
  filter_group->MixAndFilter(input_samples);

  // Verify if the fiter group output matches the source after down mixing.
  float* interleaved_data = filter_group->interleaved();
  const int output_chunk_size = input_samples * num_output_channel;
  for (int i = 0; i < output_chunk_size; ++i) {
    const float* left_input = input->data()->channel(0);
    const float* right_input = input->data()->channel(1);
    ASSERT_EQ((left_input[i] + right_input[i]) / 2, interleaved_data[i]);
  }
}

}  // namespace media
}  // namespace chromecast
