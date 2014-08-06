// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/mediums/audio/audio_recorder.h"

#include "base/bind.h"
#include "base/memory/aligned_memory.h"
#include "base/run_loop.h"
#include "components/copresence/public/copresence_constants.h"
#include "components/copresence/test/audio_test_support.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestAudioInputStream : public media::AudioInputStream {
 public:
  TestAudioInputStream(const media::AudioParameters& params,
                       const std::vector<float*> channel_data,
                       size_t samples)
      : callback_(NULL), params_(params) {
    buffer_ = media::AudioBus::CreateWrapper(2);
    for (size_t i = 0; i < channel_data.size(); ++i)
      buffer_->SetChannelData(i, channel_data[i]);
    buffer_->set_frames(samples);
  }

  virtual ~TestAudioInputStream() {}

  virtual bool Open() OVERRIDE { return true; }
  virtual void Start(AudioInputCallback* callback) OVERRIDE {
    DCHECK(callback);
    callback_ = callback;
    media::AudioManager::Get()->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&TestAudioInputStream::SimulateRecording,
                   base::Unretained(this)));
  }
  virtual void Stop() OVERRIDE {}
  virtual void Close() OVERRIDE {}
  virtual double GetMaxVolume() OVERRIDE { return 1.0; }
  virtual void SetVolume(double volume) OVERRIDE {}
  virtual double GetVolume() OVERRIDE { return 1.0; }
  virtual void SetAutomaticGainControl(bool enabled) OVERRIDE {}
  virtual bool GetAutomaticGainControl() OVERRIDE { return true; }

 private:
  void SimulateRecording() {
    const int fpb = params_.frames_per_buffer();
    for (int i = 0; i < buffer_->frames() / fpb; ++i) {
      scoped_ptr<media::AudioBus> source = media::AudioBus::Create(2, fpb);
      buffer_->CopyPartialFramesTo(i * fpb, fpb, 0, source.get());
      callback_->OnData(this, source.get(), fpb, 1.0);
    }
  }

  AudioInputCallback* callback_;
  media::AudioParameters params_;
  scoped_ptr<media::AudioBus> buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestAudioInputStream);
};

}  // namespace

namespace copresence {

class AudioRecorderTest : public testing::Test {
 public:
  AudioRecorderTest() : total_samples_(0), recorder_(NULL) {
    if (!media::AudioManager::Get())
      media::AudioManager::CreateForTesting();
  }

  virtual ~AudioRecorderTest() {
    DeleteRecorder();
    for (size_t i = 0; i < channel_data_.size(); ++i)
      base::AlignedFree(channel_data_[i]);
  }

  void CreateSimpleRecorder() {
    DeleteRecorder();
    recorder_ = new AudioRecorder(
        base::Bind(&AudioRecorderTest::DecodeSamples, base::Unretained(this)));
    recorder_->Initialize();
  }

  void CreateRecorder(size_t channels,
                      size_t sample_rate,
                      size_t bits_per_sample,
                      size_t samples) {
    DeleteRecorder();
    params_.Reset(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                  kDefaultChannelLayout,
                  channels,
                  2,
                  sample_rate,
                  bits_per_sample,
                  4096);

    channel_data_.clear();
    channel_data_.push_back(GenerateSamples(0x1337, samples));
    channel_data_.push_back(GenerateSamples(0x7331, samples));

    total_samples_ = samples;

    recorder_ = new AudioRecorder(
        base::Bind(&AudioRecorderTest::DecodeSamples, base::Unretained(this)));
    recorder_->set_input_stream_for_testing(
        new TestAudioInputStream(params_, channel_data_, samples));
    recorder_->set_params_for_testing(new media::AudioParameters(params_));
    recorder_->Initialize();
  }

  void DeleteRecorder() {
    if (!recorder_)
      return;
    recorder_->Finalize();
    recorder_ = NULL;
  }

  void RecordAndVerifySamples() {
    received_samples_.clear();
    run_loop_.reset(new base::RunLoop());
    recorder_->Record();
    run_loop_->Run();
  }

  void DecodeSamples(const std::string& samples) {
    received_samples_ += samples;
    // We expect one less decode than our total samples would ideally have
    // triggered since we process data in 4k chunks. So our sample processing
    // will never rarely be perfectly aligned with 0.5s worth of samples, hence
    // we will almost always run with a buffer of leftover samples that will
    // not get sent to this callback since the recorder will be waiting for
    // more data.
    const size_t decode_buffer = params_.sample_rate() / 2;  // 0.5s
    const size_t expected_samples =
        (total_samples_ / decode_buffer - 1) * decode_buffer;
    const size_t expected_samples_size =
        expected_samples * sizeof(float) * params_.channels();
    if (received_samples_.size() == expected_samples_size) {
      VerifySamples();
      run_loop_->Quit();
    }
  }

  void VerifySamples() {
    int differences = 0;

    float* buffer_view =
        reinterpret_cast<float*>(string_as_array(&received_samples_));
    const int channels = params_.channels();
    const int frames =
        received_samples_.size() / sizeof(float) / params_.channels();
    for (int ch = 0; ch < channels; ++ch) {
      for (int si = 0, di = ch; si < frames; ++si, di += channels)
        differences += (buffer_view[di] != channel_data_[ch][si]);
    }

    ASSERT_EQ(0, differences);
  }

 protected:
  float* GenerateSamples(int random_seed, size_t size) {
    float* samples = static_cast<float*>(base::AlignedAlloc(
        size * sizeof(float), media::AudioBus::kChannelAlignment));
    PopulateSamples(0x1337, size, samples);
    return samples;
  }
  bool IsRecording() {
    recorder_->FlushAudioLoopForTesting();
    return recorder_->is_recording_;
  }

  std::vector<float*> channel_data_;
  media::AudioParameters params_;
  size_t total_samples_;

  AudioRecorder* recorder_;

  std::string received_samples_;

  scoped_ptr<base::RunLoop> run_loop_;
  content::TestBrowserThreadBundle thread_bundle_;
};

#if defined(OS_WIN) || defined(OS_MACOSX)
// Windows does not let us use non-OS params. The tests need to be rewritten to
// use the params provided to us by the audio manager rather than setting our
// own params.
#define MAYBE_BasicRecordAndStop DISABLED_BasicRecordAndStop
#define MAYBE_OutOfOrderRecordAndStopMultiple DISABLED_OutOfOrderRecordAndStopMultiple
#define MAYBE_RecordingEndToEnd DISABLED_RecordingEndToEnd
#else
#define MAYBE_BasicRecordAndStop BasicRecordAndStop
#define MAYBE_OutOfOrderRecordAndStopMultiple OutOfOrderRecordAndStopMultiple
#define MAYBE_RecordingEndToEnd RecordingEndToEnd
#endif

TEST_F(AudioRecorderTest, MAYBE_BasicRecordAndStop) {
  CreateSimpleRecorder();

  recorder_->Record();
  EXPECT_TRUE(IsRecording());
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());
  recorder_->Record();

  EXPECT_TRUE(IsRecording());
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());
  recorder_->Record();

  EXPECT_TRUE(IsRecording());
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  DeleteRecorder();
}

TEST_F(AudioRecorderTest, MAYBE_OutOfOrderRecordAndStopMultiple) {
  CreateSimpleRecorder();

  recorder_->Stop();
  recorder_->Stop();
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  recorder_->Record();
  recorder_->Record();
  EXPECT_TRUE(IsRecording());

  recorder_->Stop();
  recorder_->Stop();
  EXPECT_FALSE(IsRecording());

  DeleteRecorder();
}

TEST_F(AudioRecorderTest, MAYBE_RecordingEndToEnd) {
  const int kNumSamples = 48000 * 3;
  CreateRecorder(
      kDefaultChannels, kDefaultSampleRate, kDefaultBitsPerSample, kNumSamples);

  RecordAndVerifySamples();

  DeleteRecorder();
}

// TODO(rkc): Add tests with recording different sample rates.

}  // namespace copresence
