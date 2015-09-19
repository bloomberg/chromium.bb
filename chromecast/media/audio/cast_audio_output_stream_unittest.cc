// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/task_runner_impl.h"
#include "chromecast/media/audio/cast_audio_manager.h"
#include "chromecast/media/audio/cast_audio_output_stream.h"
#include "chromecast/public/media/audio_pipeline_device.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/decrypt_context.h"
#include "chromecast/public/media/media_clock_device.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {
const char kDefaultDeviceId[] = "";

class FakeClockDevice : public MediaClockDevice {
 public:
  FakeClockDevice() : state_(kStateUninitialized), rate_(0.f) {}
  ~FakeClockDevice() override {}

  State GetState() const override { return state_; }
  bool SetState(State new_state) override {
    state_ = new_state;
    return true;
  }
  bool ResetTimeline(int64_t time_microseconds) override { return true; }
  bool SetRate(float rate) override {
    rate_ = rate;
    return true;
  }
  int64_t GetTimeMicroseconds() override { return 0; }

  float rate() const { return rate_; }

 private:
  State state_;
  float rate_;
};

class FakeAudioPipelineDevice : public AudioPipelineDevice {
 public:
  enum PipelineStatus {
    PIPELINE_STATUS_OK,
    PIPELINE_STATUS_BUSY,
    PIPELINE_STATUS_ERROR
  };

  FakeAudioPipelineDevice()
      : state_(kStateUninitialized),
        volume_multiplier_(1.0f),
        pipeline_status_(PIPELINE_STATUS_OK),
        pushed_frame_count_(0) {}
  ~FakeAudioPipelineDevice() override {}

  // AudioPipelineDevice overrides.
  void SetClient(Client* client) override {}
  bool SetState(State new_state) override {
    state_ = new_state;
    return true;
  }
  State GetState() const override { return state_; }
  bool SetStartPts(int64_t microseconds) override { return false; }
  FrameStatus PushFrame(DecryptContext* decrypt_context,
                        CastDecoderBuffer* buffer,
                        FrameStatusCB* completion_cb) override {
    last_frame_decrypt_context_.reset(decrypt_context);
    last_frame_buffer_.reset(buffer);
    last_frame_completion_cb_.reset(completion_cb);
    ++pushed_frame_count_;

    switch (pipeline_status_) {
      case PIPELINE_STATUS_OK:
        return kFrameSuccess;
      case PIPELINE_STATUS_BUSY:
        return kFramePending;
      case PIPELINE_STATUS_ERROR:
        return kFrameFailed;
    }
    NOTREACHED();
  }
  RenderingDelay GetRenderingDelay() const override { return RenderingDelay(); }
  bool GetStatistics(Statistics* stats) const override { return false; }
  bool SetConfig(const AudioConfig& config) override {
    config_ = config;
    return true;
  }
  void SetStreamVolumeMultiplier(float multiplier) override {
    volume_multiplier_ = multiplier;
  }

  const AudioConfig& config() const { return config_; }
  float volume_multiplier() const { return volume_multiplier_; }
  void set_pipeline_status(PipelineStatus status) { pipeline_status_ = status; }
  unsigned pushed_frame_count() const { return pushed_frame_count_; }
  DecryptContext* last_frame_decrypt_context() {
    return last_frame_decrypt_context_.get();
  }
  CastDecoderBuffer* last_frame_buffer() { return last_frame_buffer_.get(); }
  FrameStatusCB* last_frame_completion_cb() {
    return last_frame_completion_cb_.get();
  }

 private:
  State state_;
  AudioConfig config_;
  float volume_multiplier_;

  PipelineStatus pipeline_status_;
  unsigned pushed_frame_count_;
  scoped_ptr<DecryptContext> last_frame_decrypt_context_;
  scoped_ptr<CastDecoderBuffer> last_frame_buffer_;
  scoped_ptr<FrameStatusCB> last_frame_completion_cb_;
};

class FakeMediaPipelineBackend : public MediaPipelineBackend {
 public:
  ~FakeMediaPipelineBackend() override {}

  MediaClockDevice* GetClock() override {
    if (!clock_device_)
      clock_device_.reset(new FakeClockDevice);
    return clock_device_.get();
  }
  AudioPipelineDevice* GetAudio() override {
    if (!audio_device_)
      audio_device_.reset(new FakeAudioPipelineDevice);
    return audio_device_.get();
  }
  VideoPipelineDevice* GetVideo() override {
    NOTREACHED();
    return nullptr;
  }

 private:
  scoped_ptr<FakeClockDevice> clock_device_;
  scoped_ptr<FakeAudioPipelineDevice> audio_device_;
};

class FakeAudioSourceCallback
    : public ::media::AudioOutputStream::AudioSourceCallback {
 public:
  FakeAudioSourceCallback() : error_(false) {}

  bool error() const { return error_; }

  // ::media::AudioOutputStream::AudioSourceCallback overrides.
  int OnMoreData(::media::AudioBus* audio_bus,
                 uint32 total_bytes_delay) override {
    audio_bus->Zero();
    return audio_bus->frames();
  }
  void OnError(::media::AudioOutputStream* stream) override { error_ = true; }

 private:
  bool error_;
};

class FakeAudioManager : public CastAudioManager {
 public:
  FakeAudioManager()
      : CastAudioManager(nullptr), media_pipeline_backend_(nullptr) {}
  ~FakeAudioManager() override {}

  // CastAudioManager overrides.
  scoped_ptr<MediaPipelineBackend> CreateMediaPipelineBackend() override {
    DCHECK(!media_pipeline_backend_);
    scoped_ptr<FakeMediaPipelineBackend> backend(new FakeMediaPipelineBackend);
    // Cache the backend locally to be used by tests.
    media_pipeline_backend_ = backend.get();
    return backend.Pass();
  }
  void ReleaseOutputStream(::media::AudioOutputStream* stream) override {
    DCHECK(media_pipeline_backend_);
    media_pipeline_backend_ = nullptr;
    CastAudioManager::ReleaseOutputStream(stream);
  }

  // Returns the MediaPipelineBackend being used by the AudioOutputStream.
  // Note: here is a valid MediaPipelineBackend only while the stream is open.
  // Returns NULL at all other times.
  FakeMediaPipelineBackend* media_pipeline_backend() {
    return media_pipeline_backend_;
  }

 private:
  FakeMediaPipelineBackend* media_pipeline_backend_;
};

class AudioOutputStreamTest : public ::testing::Test {
 public:
  AudioOutputStreamTest()
      : format_(::media::AudioParameters::AUDIO_PCM_LINEAR),
        channel_layout_(::media::CHANNEL_LAYOUT_MONO),
        sample_rate_(::media::AudioParameters::kAudioCDSampleRate),
        bits_per_sample_(16),
        frames_per_buffer_(256) {}
  ~AudioOutputStreamTest() override {}

 protected:
  void SetUp() override {
    message_loop_.reset(new base::MessageLoop());
    audio_manager_.reset(new FakeAudioManager);
  }

  void TearDown() override {
    audio_manager_.reset();
    message_loop_.reset();
  }

  ::media::AudioParameters GetAudioParams() {
    return ::media::AudioParameters(format_, channel_layout_, sample_rate_,
                                    bits_per_sample_, frames_per_buffer_);
  }
  ::media::AudioOutputStream* CreateStream() {
    return audio_manager_->MakeAudioOutputStream(GetAudioParams(),
                                                 kDefaultDeviceId);
  }
  FakeClockDevice* GetClock() {
    MediaPipelineBackend* backend = audio_manager_->media_pipeline_backend();
    return backend ? static_cast<FakeClockDevice*>(backend->GetClock())
                   : nullptr;
  }
  FakeAudioPipelineDevice* GetAudio() {
    MediaPipelineBackend* backend = audio_manager_->media_pipeline_backend();
    return backend ? static_cast<FakeAudioPipelineDevice*>(backend->GetAudio())
                   : nullptr;
  }

  scoped_ptr<base::MessageLoop> message_loop_;
  scoped_ptr<FakeAudioManager> audio_manager_;
  scoped_ptr<TaskRunnerImpl> audio_task_runner_;

  // AudioParameters used to create AudioOutputStream.
  // Tests can modify these parameters before calling CreateStream.
  ::media::AudioParameters::Format format_;
  ::media::ChannelLayout channel_layout_;
  int sample_rate_;
  int bits_per_sample_;
  int frames_per_buffer_;
};

TEST_F(AudioOutputStreamTest, Format) {
  ::media::AudioParameters::Format format[] = {
      ::media::AudioParameters::AUDIO_PCM_LINEAR,
      ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY};
  for (size_t i = 0; i < arraysize(format); ++i) {
    format_ = format[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(stream->Open());

    FakeAudioPipelineDevice* audio_device = GetAudio();
    ASSERT_TRUE(audio_device);
    const AudioConfig& audio_config = audio_device->config();
    EXPECT_EQ(kCodecPCM, audio_config.codec);
    EXPECT_EQ(kSampleFormatS16, audio_config.sample_format);
    EXPECT_FALSE(audio_config.is_encrypted);

    stream->Close();
  }
}

TEST_F(AudioOutputStreamTest, ChannelLayout) {
  ::media::ChannelLayout layout[] = {::media::CHANNEL_LAYOUT_MONO,
                                     ::media::CHANNEL_LAYOUT_STEREO};
  for (size_t i = 0; i < arraysize(layout); ++i) {
    channel_layout_ = layout[i];
    ::media::AudioOutputStream* stream = CreateStream();
    ASSERT_TRUE(stream);
    EXPECT_TRUE(stream->Open());

    FakeAudioPipelineDevice* audio_device = GetAudio();
    ASSERT_TRUE(audio_device);
    const AudioConfig& audio_config = audio_device->config();
    EXPECT_EQ(::media::ChannelLayoutToChannelCount(channel_layout_),
              audio_config.channel_number);

    stream->Close();
  }
}

TEST_F(AudioOutputStreamTest, SampleRate) {
  sample_rate_ = ::media::AudioParameters::kAudioCDSampleRate;
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  const AudioConfig& audio_config = audio_device->config();
  EXPECT_EQ(sample_rate_, audio_config.samples_per_second);

  stream->Close();
}

TEST_F(AudioOutputStreamTest, BitsPerSample) {
  bits_per_sample_ = 16;
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  const AudioConfig& audio_config = audio_device->config();
  EXPECT_EQ(bits_per_sample_ / 8, audio_config.bytes_per_channel);

  stream->Close();
}

TEST_F(AudioOutputStreamTest, DeviceState) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_FALSE(GetAudio());

  EXPECT_TRUE(stream->Open());
  AudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  FakeClockDevice* clock_device = GetClock();
  ASSERT_TRUE(clock_device);
  EXPECT_EQ(AudioPipelineDevice::kStateIdle, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateIdle, clock_device->GetState());
  EXPECT_EQ(1.f, clock_device->rate());

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  stream->Start(source_callback.get());
  EXPECT_EQ(AudioPipelineDevice::kStateRunning, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateRunning, clock_device->GetState());
  EXPECT_EQ(1.f, clock_device->rate());

  stream->Stop();
  EXPECT_EQ(AudioPipelineDevice::kStatePaused, audio_device->GetState());
  EXPECT_EQ(MediaClockDevice::kStateIdle, clock_device->GetState());
  EXPECT_EQ(0.f, clock_device->rate());

  stream->Close();
  EXPECT_FALSE(GetAudio());
}

TEST_F(AudioOutputStreamTest, PushFrame) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  stream->Start(source_callback.get());

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);

  EXPECT_EQ(0u, audio_device->pushed_frame_count());
  EXPECT_FALSE(audio_device->last_frame_decrypt_context());
  EXPECT_FALSE(audio_device->last_frame_buffer());
  EXPECT_FALSE(audio_device->last_frame_completion_cb());

  // Let the stream push frames.
  message_loop_->RunUntilIdle();

  EXPECT_LT(0u, audio_device->pushed_frame_count());
  // DecryptContext is always NULL becuase of "raw" audio.
  EXPECT_FALSE(audio_device->last_frame_decrypt_context());
  EXPECT_TRUE(audio_device->last_frame_buffer());
  EXPECT_TRUE(audio_device->last_frame_completion_cb());

  // Verify decoder buffer.
  ::media::AudioParameters audio_params = GetAudioParams();
  const size_t expected_frame_size =
      static_cast<size_t>(audio_params.GetBytesPerBuffer());
  const CastDecoderBuffer* buffer = audio_device->last_frame_buffer();
  EXPECT_TRUE(buffer->data());
  EXPECT_EQ(expected_frame_size, buffer->data_size());
  EXPECT_FALSE(buffer->decrypt_config());  // Null because of raw audio.
  EXPECT_FALSE(buffer->end_of_stream());

  // No error must be reported to source callback.
  EXPECT_FALSE(source_callback->error());

  stream->Stop();
  stream->Close();
}

TEST_F(AudioOutputStreamTest, DeviceBusy) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_BUSY);

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  stream->Start(source_callback.get());

  // Let the stream push frames.
  message_loop_->RunUntilIdle();

  // Make sure that one frame was pushed.
  EXPECT_EQ(1u, audio_device->pushed_frame_count());
  // No error must be reported to source callback.
  EXPECT_FALSE(source_callback->error());

  // Sleep for a few frames so that when the message loop is drained
  // AudioOutputStream would have the opportunity to push more frames.
  ::media::AudioParameters audio_params = GetAudioParams();
  base::TimeDelta pause = audio_params.GetBufferDuration() * 5;
  base::PlatformThread::Sleep(pause);

  // Let the stream attempt to push more frames.
  message_loop_->RunUntilIdle();
  // But since the device was busy, it must not push more frames.
  EXPECT_EQ(1u, audio_device->pushed_frame_count());

  // Unblock the pipeline and verify that PushFrame resumes.
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_OK);
  audio_device->last_frame_completion_cb()->Run(
      MediaComponentDevice::kFrameSuccess);
  base::PlatformThread::Sleep(pause);
  message_loop_->RunUntilIdle();
  EXPECT_EQ(2u, audio_device->pushed_frame_count());
  EXPECT_FALSE(source_callback->error());

  // Make the pipeline busy again, but this time send kFrameFailed.
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_BUSY);
  base::PlatformThread::Sleep(pause);
  message_loop_->RunUntilIdle();
  EXPECT_EQ(3u, audio_device->pushed_frame_count());
  EXPECT_FALSE(source_callback->error());

  audio_device->last_frame_completion_cb()->Run(
      MediaComponentDevice::kFrameFailed);
  EXPECT_TRUE(source_callback->error());

  stream->Stop();
  stream->Close();
}

TEST_F(AudioOutputStreamTest, DeviceError) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  EXPECT_TRUE(stream->Open());

  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);
  audio_device->set_pipeline_status(
      FakeAudioPipelineDevice::PIPELINE_STATUS_ERROR);

  scoped_ptr<FakeAudioSourceCallback> source_callback(
      new FakeAudioSourceCallback);
  stream->Start(source_callback.get());

  // Let the stream push frames.
  message_loop_->RunUntilIdle();

  // Make sure that AudioOutputStream attempted to push the initial frame.
  EXPECT_LT(0u, audio_device->pushed_frame_count());
  // AudioOutputStream must report error to source callback.
  EXPECT_TRUE(source_callback->error());

  stream->Stop();
  stream->Close();
}

TEST_F(AudioOutputStreamTest, Volume) {
  ::media::AudioOutputStream* stream = CreateStream();
  ASSERT_TRUE(stream);
  ASSERT_TRUE(stream->Open());
  FakeAudioPipelineDevice* audio_device = GetAudio();
  ASSERT_TRUE(audio_device);

  double volume = 0.0;
  stream->GetVolume(&volume);
  EXPECT_EQ(1.0, volume);
  EXPECT_EQ(1.0f, audio_device->volume_multiplier());

  stream->SetVolume(0.5);
  stream->GetVolume(&volume);
  EXPECT_EQ(0.5, volume);
  EXPECT_EQ(0.5f, audio_device->volume_multiplier());

  stream->Close();
}

}  // namespace
}  // namespace media
}  // namespace chromecast
