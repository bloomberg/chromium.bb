// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/alsa/stream_mixer_alsa.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/media/cma/backend/alsa/mock_alsa_wrapper.h"
#include "media/base/audio_bus.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace chromecast {
namespace media {

namespace {

// Testing constants that are common to multiple test cases.
const int kNumChannels = 2;
const int kBytesPerSample = 4;
const int kTestMaxReadSize = 4096;
const int kTestSamplesPerSecond = 12345;

// This array holds |NUM_DATA_SETS| sets of arbitrary interleaved float data.
// Each set holds |NUM_SAMPLES| / kNumChannels frames of data.
#define NUM_DATA_SETS 2u
#define NUM_SAMPLES 64u

float kTestData[NUM_DATA_SETS][NUM_SAMPLES] = {
  {
     0.034619, -0.62082,
    -0.633705,  0.530298,
     0.89957,   0.864411,
     0.302309,  0.262931,
     0.780742,  0.0109042,
    -0.602219,  0.255149,
    -0.454606,  0.857073,
     0.798388,  0.167035,
     0.458046,  0.588998,
     0.206043,  0.569494,
     0.147803,  0.170857,
     0.648797, -0.475908,
    -0.95678,  -0.0743951,
     0.524809, -0.924101,
     0.65518,  -0.322856,
    -0.721563,  0.573805,
     0.451849, -0.814982,
    -0.364712,  0.573464,
     0.537977, -0.381851,
     0.53816,  -0.516168,
    -0.0700984, 0.481362,
     0.98778,   0.852158,
    -0.8815,   -0.381422,
    -0.230589,  0.466485,
    -0.67107,   0.322319,
     0.892472, -0.320276,
    -0.424015,  0.789646,
     0.462945,  0.826759,
     0.423481,  0.229418,
    -0.354715, -0.961272,
     0.632236, -0.735754,
     0.872045, -0.709881,
  }, {
     0.908805,  0.331583,
     0.514689,  0.803509,
    -0.459579,  0.106887,
     0.48557,   0.879381,
     0.688156,  0.692105,
    -0.158415, -0.851542,
     0.660676, -0.33735,
    -0.49228,   0.655911,
    -0.014641,  0.66197,
    -0.573276,  0.254189,
     0.666018,  0.98153,
    -0.967202, -0.525433,
    -0.838106, -0.484799,
     0.889906, -0.548501,
    -0.889936, -0.432651,
     0.590209,  0.0801954,
    -0.953734, -0.994462,
     0.341957,  0.565746,
     0.605983, -0.152254,
    -0.232025, -0.54384,
    -0.274306,  0.275652,
     0.276137, -0.367112,
    -0.000647,  0.68271,
     0.179821,  0.646818,
     0.435898, -0.669994,
     0.624382, -0.752883,
    -0.396175,  0.776021,
     0.600866, -0.2293,
     0.306964, -0.252563,
    -0.055882,  0.480061,
    -0.408798, -0.405238,
     0.61592,  -0.16056,
  }
};

// Return a scoped pointer filled with the data laid out at |index| above.
scoped_ptr<::media::AudioBus> GetTestData(size_t index) {
  CHECK_LT(index, NUM_DATA_SETS);
  int frames = NUM_SAMPLES / kNumChannels;
  auto data =
      ::media::AudioBus::WrapMemory(kNumChannels, frames, kTestData[index]);
  return data;
}

class MockInputQueue : public StreamMixerAlsa::InputQueue {
 public:
  explicit MockInputQueue(int samples_per_second)
      : paused_(true),
        samples_per_second_(samples_per_second),
        max_read_size_(kTestMaxReadSize),
        multiplier_(1.0),
        primary_(true),
        deleting_(false) {
    ON_CALL(*this, GetResampledData(_, _)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoGetResampledData));
    ON_CALL(*this, PrepareToDelete(_)).WillByDefault(
        testing::Invoke(this, &MockInputQueue::DoPrepareToDelete));
  }
  ~MockInputQueue() override {}

  bool paused() const { return paused_; }

  // StreamMixerAlsa::InputQueue implementation:
  int input_samples_per_second() const override { return samples_per_second_; }
  float volume_multiplier() const override { return multiplier_; }
  bool primary() const override { return primary_; }
  bool IsDeleting() const override { return deleting_; }
  MOCK_METHOD1(Initialize,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  int MaxReadSize() override { return max_read_size_; }
  MOCK_METHOD2(GetResampledData, void(::media::AudioBus* dest, int frames));
  MOCK_METHOD1(AfterWriteFrames,
               void(const MediaPipelineBackendAlsa::RenderingDelay&
                        mixer_rendering_delay));
  MOCK_METHOD0(SignalError, void());
  MOCK_METHOD1(PrepareToDelete, void(const OnReadyToDeleteCb& delete_cb));

  // Setters and getters for test control.
  void SetPaused(bool paused) { paused_ = paused; }
  void SetMaxReadSize(int max_read_size) { max_read_size_ = max_read_size; }
  void SetData(scoped_ptr<::media::AudioBus> data) {
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
    if (data_)
      data_->CopyPartialFramesTo(0, frames, 0, dest);
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
  scoped_ptr<::media::AudioBus> data_;

  DISALLOW_COPY_AND_ASSIGN(MockInputQueue);
};

// Given |inputs|, returns mixed audio data according to the mixing method used
// by the mixer.
scoped_ptr<::media::AudioBus> GetMixedAudioData(
    const std::vector<testing::StrictMock<MockInputQueue>*>& inputs) {
  int read_size = std::numeric_limits<int>::max();
  for (const auto input : inputs) {
    CHECK(input);
    read_size = std::min(input->MaxReadSize(), read_size);
  }

  // Verify all inputs are the right size.
  for (const auto input : inputs) {
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
      for (const auto input : inputs)
        *result += *(input->data().channel(c) + f) * input->multiplier();

      // Clamp the mixed sample between 1.0 and -1.0.
      *result = std::min(1.0f, std::max(-1.0f, *result));
    }
  }
  return mixed;
}

// Like the method above, but accepts a single input. This returns an AudioBus
// with this input after it is scaled and clipped.
scoped_ptr<::media::AudioBus> GetMixedAudioData(
    testing::StrictMock<MockInputQueue>* input) {
  return GetMixedAudioData(
      std::vector<testing::StrictMock<MockInputQueue>*>(1, input));
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

}  // namespace

class StreamMixerAlsaTest : public testing::Test {
 protected:
  StreamMixerAlsaTest()
      : message_loop_(new base::MessageLoop()),
        mock_alsa_(new testing::NiceMock<MockAlsaWrapper>()) {
    StreamMixerAlsa::MakeSingleThreadedForTest();
    StreamMixerAlsa::Get()->SetAlsaWrapperForTest(make_scoped_ptr(mock_alsa_));
  }

  ~StreamMixerAlsaTest() override {
    StreamMixerAlsa::Get()->ClearInputsForTest();
  }

  MockAlsaWrapper* mock_alsa() { return mock_alsa_; }

 private:
  const scoped_ptr<base::MessageLoop> message_loop_;
  testing::NiceMock<MockAlsaWrapper>* mock_alsa_;

  DISALLOW_COPY_AND_ASSIGN(StreamMixerAlsaTest);
};

TEST_F(StreamMixerAlsaTest, AddSingleInput) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(make_scoped_ptr(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());
}

TEST_F(StreamMixerAlsaTest, AddMultipleInputs) {
  auto input1 = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  auto input2 =
      new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond * 2);
  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();

  EXPECT_CALL(*input1, Initialize(_)).Times(1);
  EXPECT_CALL(*input2, Initialize(_)).Times(1);
  mixer->AddInput(make_scoped_ptr(input1));
  mixer->AddInput(make_scoped_ptr(input2));

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
    mixer->AddInput(make_scoped_ptr(inputs[i]));
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
    mixer->AddInput(make_scoped_ptr(inputs[i]));
  }

  ASSERT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The mixer should pull data from all streams, using the smallest
  // MaxReadSize provided by any of the channels.
  // TODO(slan): Check that the proper number of frames is pulled.
  ASSERT_EQ(3u, inputs.size());
  inputs[0]->SetMaxReadSize(1024);
  inputs[1]->SetMaxReadSize(512);
  inputs[2]->SetMaxReadSize(2048);
  for (const auto input : inputs) {
    EXPECT_CALL(*input, GetResampledData(_, 512)).Times(1);
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }

  // TODO(slan): Verify that the data is mixed properly with math.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 512)).Times(1);
  mixer->WriteFramesForTest();

  // Make two of these streams non-primary, and exhaust a non-primary stream.
  // All non-empty streams shall be polled for data and the mixer shall write
  // to ALSA.
  inputs[1]->SetPrimary(false);
  inputs[1]->SetMaxReadSize(0);
  inputs[2]->SetPrimary(false);
  for (const auto input : inputs) {
    if (input != inputs[1])
      EXPECT_CALL(*input, GetResampledData(_, 1024)).Times(1);
    EXPECT_CALL(*input, AfterWriteFrames(_)).Times(1);
  }
  // Note that the new smallest stream shall dictate the length of the write.
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 1024)).Times(1);
  mixer->WriteFramesForTest();

  // Exhaust a primary stream. No streams shall be polled for data, and no
  // data shall be written to ALSA.
  inputs[0]->SetMaxReadSize(0);
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, _)).Times(0);
  mixer->WriteFramesForTest();
}

TEST_F(StreamMixerAlsaTest, OneStreamMixesProperly) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(make_scoped_ptr(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  input->SetData(GetTestData(0));

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GT(mock_alsa()->data().size(), 0u);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  auto expected = GetMixedAudioData(input);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, OneStreamIsScaledDownProperly) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(make_scoped_ptr(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // Populate the stream with data.
  const int kNumFrames = 32;
  ASSERT_EQ((int)sizeof(kTestData[0]),
            kNumChannels * kNumFrames * kBytesPerSample);
  auto data =
      ::media::AudioBus::WrapMemory(kNumChannels, kNumFrames, kTestData[0]);
  input->SetData(std::move(data));

  // Set a volume multiplier on the stream.
  input->SetVolumeMultiplier(0.75);

  // Write the stream to ALSA.
  EXPECT_CALL(*input, GetResampledData(_, kNumFrames));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  mixer->WriteFramesForTest();

  // Check that the retrieved stream is scaled correctly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
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
    mixer->AddInput(make_scoped_ptr(inputs[i]));
  }

  // Poll the inputs for data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Mix the inputs manually.
  auto expected = GetMixedAudioData(inputs);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
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
    mixer->AddInput(make_scoped_ptr(inputs[i]));
  }

  // Create edge case data for the inputs. By mixing these two short streams,
  // every combination of {-1.0, 0.0, 1.0} is tested. Note that this test case
  // is intended to be a hand-checkable gut check.
  const int kNumFrames = 3;

  float kEdgeData[2][8] = {
    {
      -1.0, -1.0,
      -1.0,  0.0,
       0.0,  1.0,
       0.0,  0.0,
    }, {
      -1.0, 0.0,
       1.0, 0.0,
       1.0, 1.0,
       0.0, 0.0,
    }
  };

  // Hand-calculate the results. Index 0 is clamped to -1.0 from -2.0. Index
  // 5 is clamped from 2.0 to 1.0.
  float kResult[8] = {
    -1.0, -1.0,
     0.0,  0.0,
     1.0,  1.0,
     0.0,  0.0,
  };

  for (size_t i = 0; i < inputs.size(); ++i) {
    auto test_data =
        ::media::AudioBus::WrapMemory(kNumChannels, kNumFrames, kEdgeData[i]);
    inputs[i]->SetData(std::move(test_data));
    EXPECT_CALL(*inputs[i], GetResampledData(_, kNumFrames));
    EXPECT_CALL(*inputs[i], AfterWriteFrames(_));
  }

  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, kNumFrames)).Times(1);
  mixer->WriteFramesForTest();

  // Use the hand-calculated results above.
  auto expected =
      ::media::AudioBus::WrapMemory(kNumChannels, kNumFrames, kResult);

  // Get the actual stream rendered to ALSA, and compare it against the
  // expected stream. The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  actual->FromInterleaved(
      &(mock_alsa()->data()[0]), kNumFrames, kBytesPerSample);
  CompareAudioData(*expected, *actual);
}

TEST_F(StreamMixerAlsaTest, WriteBuffersOfVaryingLength) {
  auto input = new testing::StrictMock<MockInputQueue>(kTestSamplesPerSecond);
  input->SetPaused(false);

  StreamMixerAlsa* mixer = StreamMixerAlsa::Get();
  EXPECT_CALL(*input, Initialize(_)).Times(1);
  mixer->AddInput(make_scoped_ptr(input));
  EXPECT_EQ(StreamMixerAlsa::kStateNormalPlayback, mixer->state());

  // The input stream will provide buffers of several different lengths.
  input->SetMaxReadSize(7);
  EXPECT_CALL(*input, GetResampledData(_, 7));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 7)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(100);
  EXPECT_CALL(*input, GetResampledData(_, 100));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 100)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(32);
  EXPECT_CALL(*input, GetResampledData(_, 32));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 32)).Times(1);
  mixer->WriteFramesForTest();

  input->SetMaxReadSize(1024);
  EXPECT_CALL(*input, GetResampledData(_, 1024));
  EXPECT_CALL(*input, AfterWriteFrames(_));
  EXPECT_CALL(*mock_alsa(), PcmWritei(_, _, 1024)).Times(1);
  mixer->WriteFramesForTest();
}

}  // namespace media
}  // namespace chromecast
