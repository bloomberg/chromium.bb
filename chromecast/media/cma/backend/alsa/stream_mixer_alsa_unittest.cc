// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromecast/media/cma/backend/alsa/mock_alsa_wrapper.h"
#include "chromecast/media/cma/backend/alsa/post_processing_pipeline.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/vector_math.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromecast {
namespace media {

namespace {

using FloatType = ::media::Float32SampleTypeTraits;

// Testing constants that are common to multiple test cases.
const size_t kBytesPerSample = sizeof(int32_t);
const int kNumChannels = 2;
const int kTestMaxReadSize = 4096;

// kTestSamplesPerSecond needs to be higher than kLowSampleRateCutoff for the
// mixer to use it.
const int kTestSamplesPerSecond = 48000;

// Stream mixer alsa will never pull more than this many frames at a time.
const int kMaxWriteSizeMs = 20;
const int kMaxChunkSize = kTestSamplesPerSecond * kMaxWriteSizeMs / 1000;

// This array holds |NUM_DATA_SETS| sets of arbitrary interleaved float data.
// Each set holds |NUM_SAMPLES| / kNumChannels frames of data.
#define NUM_DATA_SETS 2u
#define NUM_SAMPLES 64u

// Note: Test data should be represented as 32-bit integers and copied into
// ::media::AudioBus instances, rather than wrapping statically declared float
// arrays. The latter method is brittle, as ::media::AudioBus requires 16-bit
// alignment for internal data.
const int32_t kTestData[NUM_DATA_SETS][NUM_SAMPLES] = {
  {
     74343736,   -1333200799,
    -1360871126,  1138806283,
     1931811865,  1856308487,
     649203634,   564640023,
     1676630678,  23416591,
    -1293255456,  547928305,
    -976258952,   1840550252,
     1714525174,  358704931,
     983646295,   1264863573,
     442473973,   1222979052,
     317404525,   366912613,
     1393280948, -1022004648,
    -2054669405, -159762261,
     1127018745, -1984491787,
     1406988336, -693327981,
    -1549544744,  1232236854,
     970338338,  -1750160519,
    -783213057,   1231504562,
     1155296810, -820018779,
     1155689800, -1108462340,
    -150535168,   1033717023,
     2121241397,  1829995370,
    -1893006836, -819097508,
    -495186107,   1001768909,
    -1441111852,  692174781,
     1916569026, -687787473,
    -910565280,   1695751872,
     994166817,   1775451433,
     909418522,   492671403,
    -761744663,  -2064315902,
     1357716471, -1580019684,
     1872702377, -1524457840,
  }, {
     1951643876,  712069070,
     1105286211,  1725522438,
    -986938388,   229538084,
     1042753634,  1888456317,
     1477803757,  1486284170,
    -340193623,  -1828672521,
     1418790906, -724453609,
    -1057163251,  1408558147,
    -31441309,    1421569750,
    -1231100836,  545866721,
     1430262764,  2107819625,
    -2077050480, -1128358776,
    -1799818931, -1041097926,
     1911058583, -1177896929,
    -1911123008, -929110948,
     1267464176,  172218310,
    -2048128170, -2135590884,
     734347065,   1214930283,
     1301338583, -326962976,
    -498269894,  -1167887508,
    -589067650,   591958162,
     592999692,  -788367017,
    -1389422,     1466108561,
     386162657,   1389031078,
     936083827,  -1438801160,
     1340850135, -1616803932,
    -850779335,   1666492408,
     1290349909, -492418001,
     659200170,  -542374913,
    -120005682,   1030923147,
    -877887021,  -870241979,
     1322678128, -344799975,
  }
};

// Compensate for integer arithmatic errors.
const int kMaxDelayErrorUs = 2;

const char kDelayModuleSolib[] = "delay.so";

// Should match # of "processors" blocks below.
const int kNumPostProcessors = 5;
const char kTestPipelineJsonTemplate[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }, {
      "streams": [ "assistant-tts" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }, {
      "streams": [ "communications" ],
      "processors": []
    }],
    "mix": {
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
       }]
    },
    "linearize": {
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }
  }
}
)json";

const int kDefaultProcessorDelay = 10;
const int kTtsProcessorDelay = 100;
const int kMixProcessorDelay = 1000;
const int kLinearizeProcessorDelay = 10000;

// Return a scoped pointer filled with the data laid out at |index| above.
std::unique_ptr<::media::AudioBus> GetTestData(size_t index) {
  CHECK_LT(index, NUM_DATA_SETS);
  int frames = NUM_SAMPLES / kNumChannels;
  auto data = ::media::AudioBus::Create(kNumChannels, frames);
  data->FromInterleaved(kTestData[index], frames, kBytesPerSample);
  return data;
}

class MockInputQueue : public StreamMixerAlsa::InputQueue {
 public:
  MockInputQueue(int samples_per_second,
                 const std::string& device_id =
                     ::media::AudioDeviceDescription::kDefaultDeviceId)
      : paused_(true),
        samples_per_second_(samples_per_second),
        max_read_size_(kTestMaxReadSize),
        multiplier_(1.0),
        primary_(true),
        deleting_(false),
        device_id_(device_id),
        filter_group_(nullptr) {
    ON_CALL(*this, GetResampledData(_, _)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoGetResampledData));
    ON_CALL(*this, VolumeScaleAccumulate(_, _, _, _)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoVolumeScaleAccumulate));
    ON_CALL(*this, PrepareToDelete(_)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoPrepareToDelete));
  }
  ~MockInputQueue() override {}

  bool paused() const { return paused_; }

  // StreamMixerAlsa::InputQueue implementation:
  int input_samples_per_second() const override { return samples_per_second_; }
  bool primary() const override { return primary_; }
  bool IsDeleting() const override { return deleting_; }
  MOCK_METHOD1(Initialize,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  std::string device_id() const override { return device_id_; }
  AudioContentType content_type() const override {
    return AudioContentType::kMedia;
  }
  void set_filter_group(FilterGroup* group) override { filter_group_ = group; }
  FilterGroup* filter_group() override { return filter_group_; }
  int MaxReadSize() override { return max_read_size_; }
  MOCK_METHOD2(GetResampledData, void(::media::AudioBus* dest, int frames));
  MOCK_METHOD4(
      VolumeScaleAccumulate,
      void(bool repeat_transition, const float* src, int frames, float* dest));
  MOCK_METHOD0(OnSkipped, void());
  MOCK_METHOD1(AfterWriteFrames,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  MOCK_METHOD1(SignalError, void(StreamMixerAlsaInput::MixerError error));
  MOCK_METHOD1(PrepareToDelete, void(const OnReadyToDeleteCb& delete_cb));

  void SetContentTypeVolume(float volume, int fade_ms) override {}
  void SetMuted(bool muted) override {}
  float EffectiveVolume() override { return multiplier_; }

  // Setters and getters for test control.
  void SetPaused(bool paused) { paused_ = paused; }
  void SetMaxReadSize(int max_read_size) { max_read_size_ = max_read_size; }
  void SetData(std::unique_ptr<::media::AudioBus> data) {
    CHECK(!data_);
    data_ = std::move(data);
    max_read_size_ = data_->frames();
  }
  void SetVolumeMultiplier(float multiplier) {
    CHECK(multiplier >= 0.0 && multiplier <= 1.0);
    multiplier_ = multiplier;
  }
  void SetPrimary(bool primary) { primary_ = primary; }
  const ::media::AudioBus& data() {
    CHECK(data_);
    return *data_;
  }
  float multiplier() const { return multiplier_; }

 private:
  void DoGetResampledData(::media::AudioBus* dest, int frames) {
    CHECK(dest);
    CHECK_GE(dest->frames(), frames);
    if (data_) {
      data_->CopyPartialFramesTo(0, frames, 0, dest);
    } else {
      dest->ZeroFramesPartial(0, frames);
    }
  }

  void DoVolumeScaleAccumulate(bool repeat_transition,
                               const float* src,
                               int frames,
                               float* dest) {
    CHECK(src);
    CHECK(dest);
    CHECK(multiplier_ >= 0.0 && multiplier_ <= 1.0);
    ::media::vector_math::FMAC(src, multiplier_, frames, dest);
  }

  void DoPrepareToDelete(const OnReadyToDeleteCb& delete_cb) {
    deleting_ = true;
    delete_cb.Run(this);
  }

  bool paused_;
  int samples_per_second_;
  int max_read_size_;
  float multiplier_;
  bool primary_;
  bool deleting_;
  const std::string device_id_;
  FilterGroup* filter_group_;

  std::unique_ptr<::media::AudioBus> data_;

  DISALLOW_COPY_AND_ASSIGN(MockInputQueue);
};

class MockPostProcessor : public PostProcessingPipeline {
 public:
  MockPostProcessor(const std::string& name,
                    const base::ListValue* filter_description_list,
                    int channels)
      : name_(name) {
    CHECK(instances_.insert({name_, this}).second);

    if (!filter_description_list) {
      // This happens for PostProcessingPipeline with no post-processors.
      return;
    }

    // Parse |filter_description_list| for parameters.
    for (size_t i = 0; i < filter_description_list->GetSize(); ++i) {
      const base::DictionaryValue* description_dict;
      CHECK(filter_description_list->GetDictionary(i, &description_dict));
      std::string solib;
      CHECK(description_dict->GetString("processor", &solib));
      // This will initially be called with the actual pipeline on creation.
      // Ignore and wait for the call to ResetPostProcessorsForTest.
      if (solib == kDelayModuleSolib) {
        const base::DictionaryValue* processor_config_dict;
        CHECK(
            description_dict->GetDictionary("config", &processor_config_dict));
        int module_delay;
        CHECK(processor_config_dict->GetInteger("delay", &module_delay));
        rendering_delay_ += module_delay;
        processor_config_dict->GetBoolean("ringing", &ringing_);
      }
    }
    ON_CALL(*this, ProcessFrames(_, _, _, _))
        .WillByDefault(
            testing::Invoke(this, &MockPostProcessor::DoProcessFrames));
  }
  ~MockPostProcessor() override { instances_.erase(name_); }
  MOCK_METHOD4(
      ProcessFrames,
      int(float* data, int num_frames, float current_volume, bool is_silence));
  bool SetSampleRate(int sample_rate) override { return false; }
  bool IsRinging() override { return ringing_; }
  int delay() { return rendering_delay_; }
  std::string name() const { return name_; }

  MOCK_METHOD2(SetPostProcessorConfig,
               void(const std::string& name, const std::string& config));

  static std::unordered_map<std::string, MockPostProcessor*>* instances() {
    return &instances_;
  }

 private:
  int DoProcessFrames(float* data,
                      int num_frames,
                      float current_volume,
                      bool is_sience) {
    return rendering_delay_;
  }

  static std::unordered_map<std::string, MockPostProcessor*> instances_;
  std::string name_;
  int rendering_delay_ = 0;
  bool ringing_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockPostProcessor);
};

std::unordered_map<std::string, MockPostProcessor*>
    MockPostProcessor::instances_;

// Given |inputs|, returns mixed audio data according to the mixing method used
// by the mixer.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    const std::vector<testing::StrictMock<MockInputQueue>*>& inputs) {
  int read_size = std::numeric_limits<int>::max();
  for (auto* input : inputs) {
    CHECK(input);
    read_size = std::min(input->MaxReadSize(), read_size);
  }

  // Verify all inputs are the right size.
  for (auto* input : inputs) {
    CHECK_EQ(kNumChannels, input->data().channels());
    CHECK_LE(read_size, input->data().frames());
  }

  // Currently, the mixing algorithm is simply to sum the scaled, clipped input
  // streams. Go sample-by-sample and mix the data.
  auto mixed = ::media::AudioBus::Create(kNumChannels, read_size);
  for (int c = 0; c < mixed->channels(); ++c) {
    for (int f = 0; f < read_size; ++f) {
      float* result = mixed->channel(c) + f;

      // Sum the sample from each input stream, scaling each stream.
      *result = 0.0;
      for (auto* input : inputs)
        *result += *(input->data().channel(c) + f) * input->multiplier();

      // Clamp the mixed sample between 1.0 and -1.0.
      *result = std::min(1.0f, std::max(-1.0f, *result));
    }
  }
  return mixed;
}

// Like the method above, but accepts a single input. This returns an AudioBus
// with this input after it is scaled and clipped.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    testing::StrictMock<MockInputQueue>* input) {
  return GetMixedAudioData(
      std::vector<testing::StrictMock<MockInputQueue>*>(1, input));
}

void ToPlanar(const uint8_t* interleaved,
              int num_frames,
              ::media::AudioBus* planar) {
  ASSERT_GE(planar->frames(), num_frames);

  planar->FromInterleaved<FloatType>(
      reinterpret_cast<const float*>(interleaved), num_frames);
}

// Asserts that |expected| matches |actual| exactly.
void CompareAudioData(const ::media::AudioBus& expected,
                      const ::media::AudioBus& actual) {
  ASSERT_EQ(expected.channels(), actual.channels());
  ASSERT_EQ(expected.frames(), actual.frames());
  for (int c = 0; c < expected.channels(); ++c) {
    const float* expected_data = expected.channel(c);
    const float* actual_data = actual.channel(c);
    for (int f = 0; f < expected.frames(); ++f)
      ASSERT_FLOAT_EQ(*expected_data++, *actual_data++) << c << " " << f;
  }
}

// Check that MediaPipelineBackendAlsa::RenderingDelay.delay_microseconds is
// within kMaxDelayErrorUs of |delay|
MATCHER_P2(MatchDelay, delay, id, "") {
  bool result = std::abs(arg.delay_microseconds - delay) < kMaxDelayErrorUs;
  if (!result) {
    LOG(ERROR) << "Expected delay_microseconds for " << id << " to be " << delay
               << " but got " << arg.delay_microseconds;
  }
  return result;
}

// Convert a number of frames at kTestSamplesPerSecond to microseconds
int64_t FramesToDelayUs(int64_t frames) {
  return frames * base::Time::kMicrosecondsPerSecond / kTestSamplesPerSecond;
}

}  // namespace

std::unique_ptr<PostProcessingPipeline> PostProcessingPipeline::Create(
    const std::string& name,
    const base::ListValue* filter_description_list,
    int channels) {
  return base::MakeUnique<testing::NiceMock<MockPostProcessor>>(
      name, filter_description_list, channels);
}

class StreamMixerAlsaTest : public testing::Test {
 protected:
  StreamMixerAlsaTest()
      : message_loop_(new base::MessageLoop()),
        mock_alsa_(new testing::NiceMock<MockAlsaWrapper>()) {
    StreamMixerAlsa::MakeSingleThreadedForTest();
    std::string test_pipeline_json = base::StringPrintf(
        kTestPipelineJsonTemplate, kDelayModuleSolib, kDefaultProcessorDelay,
        kDelayModuleSolib, kTtsProcessorDelay, kDelayModuleSolib,
        kMixProcessorDelay, kDelayModuleSolib, kLinearizeProcessorDelay);
    StreamMixerAlsa::Get()->ResetPostProcessorsForTest(test_pipeline_json);
    CHECK_EQ(MockPostProcessor::instances()->size(),
             static_cast<size_t>(kNumPostProcessors));
    StreamMixerAlsa::Get()->SetAlsaWrapperForTest(base::WrapUnique(mock_alsa_));
  }

  ~StreamMixerAlsaTest() override {
    StreamMixerAlsa::Get()->ClearInputsForTest();
    StreamMixerAlsa::Get()->SetAlsaWrapperForTest(nullptr);
  }

  MockAlsaWrapper* mock_alsa() { return mock_alsa_; }

 private:
  const std::unique_ptr<base::MessageLoop> message_loop_;
  testing::NiceMock<MockAlsaWrapper>* mock_alsa_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaTest);
};

TEST_F(StreamMixerAlsaTest, AddSingleInput) {
  auto* input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, AddMultipleInputs) {
  auto* input1 = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  auto* input2 =
      new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond * 2);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input1, Initialize(_)).Times(1);
  EXPECT_CALL(*input2, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input1));
  mixer->AddInput(base::WrapUnique(input2));

  // The mixer should be ready to play, and should sample to the initial
  // sample rate.
  EXPECT_EQ(kTestSamplesPerSecond, mixer->output_samples_per_second());
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, RemoveInput) {
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(new testing::StrictMock<MockInputQueue>(
        kTestSamplesPerSecond * (i + 1)));
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], PrepareToDelete(_)).Times(1);
    mixer->RemoveInput(inputs[i]);
  }

  // Need to wait for the removal task (it is always posted).
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(mixer->empty());
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, WriteFrames) {
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  ASSERT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The mixer should pull data from all streams, using the smallest
  // MaxReadSize provided by any of the channels.
  // TODO(slan): Check that the proper number of frames is pulled.
  ASSERT_EQ(3u, inputs.size());
  inputs[0]->SetMaxReadSize(kMaxChunkSize + 1);
  inputs[1]->SetMaxReadSize(kMaxChunkSize - 1);
  inputs[2]->SetMaxReadSize(kMaxChunkSize * 2);
  for (auto* input : inputs) {
    EXPECT_CALL(*input, GetResampledData(_, kMaxChunkSize - 1)).Times(1);
    EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kMaxChunkSize - 1, _))
        .Times(kNumChannels);
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }

  // TODO(slan): Verify that the data is mixed properly with math.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kMaxChunkSize - 1)).Times(1);
  mixer->WriteFramesForTest();

  // Make two of these streams non-primary, and exhaust a non-primary stream.
  // All non-empty streams shall be polled for data and the mixer shall write
  // to ALSA.
  inputs[1]->SetPrimary(false);
  inputs[1]->SetMaxReadSize(0);
  EXPECT_CALL(*inputs[1], OnSkipped());
  inputs[2]->SetPrimary(false);
  for (auto* input : inputs) {
    if (input != inputs[1]) {
      EXPECT_CALL(*input, GetResampledData(_, kMaxChunkSize)).Times(1);
      EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kMaxChunkSize, _))
          .Times(kNumChannels);
    }
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }
  // Note that the new smallest stream shall dictate the length of the write.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kMaxChunkSize)).Times(1);
  mixer->WriteFramesForTest();

  // Exhaust a primary stream. No streams shall be polled for data, and no
  // data shall be written to ALSA.
  inputs[0]->SetMaxReadSize(0);
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, _)).Times(0);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, OneStreamMixesProperly) {
  auto* input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  input->SetData(GetTestData(0));

  ASSERT_EQ(mock_alsa()->data().size(), 0u);

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kNumFrames, _))
      .Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GT(mock_alsa()->data().size(), 0u);
  ToPlanar(&(mock_alsa()->data()[0]), kNumFrames, actual.get());
  CompareAudioData(input->data(), *actual);
}

TEST_F(StreamMixerAlsaTest, OneStreamIsScaledDownProperly) {
  auto* input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  ASSERT_EQ(sizeof(kTestData[0]), kNumChannels * kNumFrames * kBytesPerSample);
  auto data = GetTestData(0);
  input->SetData(std::move(data));

  // Set a volume multiplier on the stream.
  input->SetVolumeMultiplier(0.75);

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kNumFrames, _))
      .Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Check that the retrieved stream is scaled correctly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ToPlanar(&(mock_alsa()->data()[0]), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(input);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, TwoUnscaledStreamsMixProperly) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Poll the inputs for data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], VolumeScaleAccumulate(_, _, kNumFrames, _))
        .Times(kNumChannels);
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Mix the inputs manually.
  auto expected = GetMixedAudioData(inputs);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ToPlanar(&(mock_alsa()->data()[0]), kNumFrames, actual.get());
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, TwoUnscaledStreamsWithDifferentIdsMixProperly) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  inputs.push_back(new testing::StrictMock<MockInputQueue>(
      kTestSamplesPerSecond,
      ::media::AudioDeviceDescription::kDefaultDeviceId));
  inputs.back()->SetPaused(false);
  inputs.push_back(new testing::StrictMock<MockInputQueue>(
      kTestSamplesPerSecond,
      ::media::AudioDeviceDescription::kCommunicationsDeviceId));
  inputs.back()->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Poll the inputs for data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], VolumeScaleAccumulate(_, _, kNumFrames, _))
        .Times(kNumChannels);
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Mix the inputs manually.
  auto expected = GetMixedAudioData(inputs);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ToPlanar(&(mock_alsa()->data()[0]), kNumFrames, actual.get());
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, TwoUnscaledStreamsMixProperlyWithEdgeCases) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Create edge case data for the inputs. By mixing these two short streams,
  // every combination of {-(2^31), 0, 2^31-1} is tested. This test case is
  // intended to be a hand-checkable gut check.
  // Note: Test data should be represented as 32-bit integers and copied into
  // ::media::AudioBus instances, rather than wrapping statically declared float
  // arrays. The latter method is brittle, as ::media::AudioBus requires 16-bit
  // alignment for internal data.
  const int kNumFrames = 3;

  const int32_t kMaxSample = std::numeric_limits<int32_t>::max();
  const int32_t kMinSample = std::numeric_limits<int32_t>::min();
  const int32_t kEdgeData[2][8] = {
    {
      kMinSample, kMinSample,
      kMinSample, 0.0,
      0.0,        kMaxSample,
      0.0,        0.0,
    }, {
      kMinSample, 0.0,
      kMaxSample, 0.0,
      kMaxSample, kMaxSample,
      0.0,        0.0,
    }
  };

  // Hand-calculate the results. Index 0 is clamped to -(2^31). Index 5 is
  // clamped to 2^31-1.
  const int32_t kResult[8] = {
    kMinSample,  kMinSample,
    0.0,         0.0,
    kMaxSample,  kMaxSample,
    0.0,         0.0,
  };

  for (size_t i = 0; i < inputs.size(); ++i) {
    auto test_data = ::media::AudioBus::Create(kNumChannels, kNumFrames);
    test_data->FromInterleaved(kEdgeData[i], kNumFrames, kBytesPerSample);
    inputs[i]->SetData(std::move(test_data));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], VolumeScaleAccumulate(_, _, kNumFrames, _))
        .Times(kNumChannels);
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Use the hand-calculated results above.
  auto expected = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  expected->FromInterleaved(kResult, kNumFrames, kBytesPerSample);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ToPlanar(&(mock_alsa()->data()[0]), kNumFrames, actual.get());
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, WriteBuffersOfVaryingLength) {
  auto* input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(base::WrapUnique(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The input stream will provide buffers of several different lengths.
  input->SetMaxReadSize(7);
  EXPECT_CALL(*input, GetResampledData(_, 7));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, 7, _)).Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 7)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(100);
  EXPECT_CALL(*input, GetResampledData(_, 100));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, 100, _)).Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 100)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(32);
  EXPECT_CALL(*input, GetResampledData(_, 32));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, 32, _)).Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 32)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(kMaxChunkSize + 1);
  EXPECT_CALL(*input, GetResampledData(_, kMaxChunkSize));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kMaxChunkSize, _))
      .Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kMaxChunkSize)).Times(1);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, StuckStreamWithoutUnderrun) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetMaxReadSize(0);
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  // Poll the inputs for data. Should not pull any data since one input has none
  // to give.
  inputs[0]->SetData(GetTestData(0));
  EXPECT_CALL(*inputs[0], GetResampledData(_, _)).Times(0);
  EXPECT_CALL(*inputs[0], VolumeScaleAccumulate(_, _, _, _)).Times(0);
  EXPECT_CALL(*inputs[0], AfterWriteFrames(_)).Times(0);

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, _)).Times(0);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, StuckStreamWithUnderrun) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetMaxReadSize(0);
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  mock_alsa()->set_state(SND_PCM_STATE_XRUN);

  // Poll the inputs for data. The first input will provide data (since the
  // output is in an underrun condition); the second input can't provide any
  // data, but AfterWriteFrames() will still be called on it so that it has the
  // correct rendering delay.
  const int kNumFrames = 32;
  inputs[0]->SetData(GetTestData(0));
  EXPECT_CALL(*inputs[0], GetResampledData(_, kNumFrames));
  EXPECT_CALL(*inputs[0], VolumeScaleAccumulate(_, _, kNumFrames, _))
      .Times(kNumChannels);
  EXPECT_CALL(*inputs[0], AfterWriteFrames(_));
  EXPECT_CALL(*inputs[1], GetResampledData(_, _)).Times(0);
  EXPECT_CALL(*inputs[1], VolumeScaleAccumulate(_, _, kNumFrames, _)).Times(0);
  EXPECT_CALL(*inputs[1], OnSkipped());
  EXPECT_CALL(*inputs[1], AfterWriteFrames(_));

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, StuckStreamWithLowBuffer) {
  // Create a group of input streams.
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond));
    inputs.back()->SetMaxReadSize(0);
    inputs.back()->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  mock_alsa()->set_avail(4086);

  // Poll the inputs for data. The first input will provide data (since the
  // output is in an low buffer condition); the second input can't provide any
  // data, but AfterWriteFrames() will still be called on it so that it has the
  // correct rendering delay.
  const int kNumFrames = 32;
  inputs[0]->SetData(GetTestData(0));
  EXPECT_CALL(*inputs[0], GetResampledData(_, kNumFrames));
  EXPECT_CALL(*inputs[0], VolumeScaleAccumulate(_, _, kNumFrames, _))
      .Times(kNumChannels);
  EXPECT_CALL(*inputs[0], AfterWriteFrames(_));
  EXPECT_CALL(*inputs[1], GetResampledData(_, _)).Times(0);
  EXPECT_CALL(*inputs[1], VolumeScaleAccumulate(_, _, _, _)).Times(0);
  EXPECT_CALL(*inputs[1], OnSkipped());
  EXPECT_CALL(*inputs[1], AfterWriteFrames(_));

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();
}

#define EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(map, name, times, frames, \
                                                silence)                  \
  do {                                                                    \
    auto itr = map->find(name);                                           \
    CHECK(itr != map->end()) << "Could not find processor for " << name;  \
    EXPECT_CALL(*(itr->second), ProcessFrames(_, frames, _, silence))     \
        .Times(times);                                                    \
  } while (0);

TEST_F(StreamMixerAlsaTest, PostProcessorDelayListedDeviceId) {
  int common_delay = kMixProcessorDelay + kLinearizeProcessorDelay;
  std::vector<testing::StrictMock<MockInputQueue>*> inputs;
  std::vector<int64_t> delays;
  inputs.push_back(new testing::StrictMock<MockInputQueue>(
      kTestSamplesPerSecond, "default"));
  delays.push_back(common_delay + kDefaultProcessorDelay);

  inputs.push_back(new testing::StrictMock<MockInputQueue>(
      kTestSamplesPerSecond, "communications"));
  delays.push_back(common_delay);

  inputs.push_back(new testing::StrictMock<MockInputQueue>(
      kTestSamplesPerSecond, "assistant-tts"));
  delays.push_back(common_delay + kTtsProcessorDelay);

  // Convert delay from frames to microseconds.
  std::transform(delays.begin(), delays.end(), delays.begin(),
                 &FramesToDelayUs);

  const int kNumFrames = 10;
  for (auto* input : inputs) {
    input->SetMaxReadSize(kNumFrames);
    input->SetPaused(false);
  }

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], Initialize(_)).Times(1);
    mixer->AddInput(base::WrapUnique(inputs[i]));
  }

  mock_alsa()->set_avail(4086);

  auto* post_processors = MockPostProcessor::instances();
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, kNumFrames,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "communications", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts", 1,
                                          kNumFrames, false);

  // Poll the inputs for data. Each input will get a different
  // rendering delay based on their device type.
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], VolumeScaleAccumulate(_, _, kNumFrames, _))
        .Times(kNumChannels);
    EXPECT_CALL(*inputs[i], AfterWriteFrames(
                                MatchDelay(delays[i], inputs[i]->device_id())));
  }
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, PostProcessorDelayUnlistedDevice) {
  const std::string device_id = "not-a-device-id";
  testing::StrictMock<MockInputQueue>* input =
      new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond, device_id);

  // Delay should be based on default processor
  int64_t delay = FramesToDelayUs(
      kDefaultProcessorDelay + kLinearizeProcessorDelay + kMixProcessorDelay);
  const int kNumFrames = 10;
  input->SetMaxReadSize(kNumFrames);
  input->SetPaused(false);

  auto* post_processors = MockPostProcessor::instances();
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, kNumFrames,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "communications", 0,
                                          _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts", 0,
                                          _, _);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_));
  mixer->AddInput(base::WrapUnique(input));

  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, VolumeScaleAccumulate(_, _, kNumFrames, _))
      .Times(kNumChannels);
  EXPECT_CALL(*input, AfterWriteFrames(MatchDelay(delay, device_id)));
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, PostProcessorRingingWithoutInput) {
  const char kTestPipelineJson[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": 0, "ringing": true}
      }]
    }, {
      "streams": [ "assistant-tts" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": 0, "ringing": true}
      }]
    }]
  }
}
)json";

  const int kNumFrames = 32;
  testing::NiceMock<MockInputQueue>* input =
      new testing::NiceMock<MockInputQueue>(kTestSamplesPerSecond, "default");
  input->SetMaxReadSize(kNumFrames);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  std::string test_pipeline_json = base::StringPrintf(
      kTestPipelineJson, kDelayModuleSolib, kDelayModuleSolib);
  mixer->ResetPostProcessorsForTest(test_pipeline_json);
  mixer->AddInput(base::WrapUnique(input));

  // "mix" + "linearize" should be automatic
  CHECK_EQ(MockPostProcessor::instances()->size(), 4u);

  mock_alsa()->set_avail(4086);

  auto* post_processors = MockPostProcessor::instances();
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, kNumFrames,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1,
                                          kNumFrames, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts", 1,
                                          kNumFrames, true);

  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, PostProcessorProvidesDefaultPipeline) {
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  mixer->ResetPostProcessorsForTest("{}");

  auto* instances = MockPostProcessor::instances();
  CHECK(instances->find("default") != instances->end());
  CHECK(instances->find("mix") != instances->end());
  CHECK(instances->find("linearize") != instances->end());
  CHECK_EQ(MockPostProcessor::instances()->size(), 3u);
}

TEST_F(StreamMixerAlsaTest, InvalidStreamTypeCrashes) {
  const char json[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "foobar" ],
      "processors": [{
        "processor": "dont_care.so",
        "config": { "delay": 0 }
      }]
    }]
  }
}
)json";

// String arguments aren't passed to CHECK() in official builds.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  EXPECT_DEATH(StreamMixerAlsa::Get()->ResetPostProcessorsForTest(json), "");
#else
  EXPECT_DEATH(StreamMixerAlsa::Get()->ResetPostProcessorsForTest(json),
               "foobar is not a stream type");
#endif
}

TEST_F(StreamMixerAlsaTest, BadJsonCrashes) {
  const std::string json("{{");
// String arguments aren't passed to CHECK() in official builds.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  EXPECT_DEATH(StreamMixerAlsa::Get()->ResetPostProcessorsForTest(json), "");
#else
  EXPECT_DEATH(StreamMixerAlsa::Get()->ResetPostProcessorsForTest(json),
               "Invalid JSON");
#endif
}

TEST_F(StreamMixerAlsaTest, MultiplePostProcessorsInOneStream) {
  const char kJsonTemplate[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "processors": [{
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 10 }
      }, {
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 100 }
      }]
    }],
    "mix": {
      "processors": [{
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 1000 }
      }, {
        "processor": "%s",
        "config": { "delay": 10000 }
      }]
    }
  }
}
)json";

  std::string json = base::StringPrintf(
      kJsonTemplate, kDelayModuleSolib, "delayer_1",  // unique processor name
      kDelayModuleSolib, "delayer_2",  // non-unique processor names
      kDelayModuleSolib, "delayer_2",
      kDelayModuleSolib  // intentionally omitted processor name
      );

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  mixer->ResetPostProcessorsForTest(json);

  // "mix" + "linearize" + "default"
  CHECK_EQ(MockPostProcessor::instances()->size(), 3u);

  auto* post_processors = MockPostProcessor::instances();
  CHECK_EQ(post_processors->find("default")->second->delay(), 110);
  CHECK_EQ(post_processors->find("mix")->second->delay(), 11000);
  CHECK_EQ(post_processors->find("linearize")->second->delay(), 0);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, SetPostProcessorConfig) {
  std::string name = "ThisIsMyName";
  std::string config = "ThisIsMyConfig";
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  auto* post_processors = MockPostProcessor::instances();

  for (auto const& x : *post_processors) {
    EXPECT_CALL(*(x.second), SetPostProcessorConfig(name, config));
  }

  mixer->SetPostProcessorConfig(name, config);
}

}  // namespace media
}  // namespace chromecast
